#include "ins_sdk.h"

#include <assert.h>
#include <algorithm>
#include <iterator>
#include <sstream>
#include <vector>
#include <boost/algorithm/string.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/sha1.hpp>
#include <gflags/gflags.h>
#include <sys/utsname.h>
#include "common/asm_atomic.h"
#include "common/mutex.h"
#include "common/this_thread.h"
#include "common/thread_pool.h"
#include "rpc/rpc_client.h"
#include "proto/ins_node.pb.h"

DECLARE_string(cluster_members);
DECLARE_int32(ins_watch_timeout);
DECLARE_int32(ins_backup_watch_timeout);

namespace galaxy {
namespace ins {
namespace sdk {

void InsSDK::ParseFlagFromArgs(int argc, char* argv[], 
                               std::vector<std::string> * members) {
    google::ParseCommandLineFlags(&argc, &argv, true);
    boost::split(*members, FLAGS_cluster_members,
                 boost::is_any_of(","), boost::token_compress_on);
    if (members->size() < 1) {
        LOG(FATAL, "invalid cluster size");
        abort();
    }
}

InsSDK::InsSDK(const std::string& server_list) { //sperated by comma
    std::vector<std::string> members;
    boost::split(members, server_list,
                 boost::is_any_of(","), boost::token_compress_on);
    Init(members);
}

InsSDK::InsSDK(const std::vector<std::string>& members) : rpc_client_(NULL),
                                                          mu_(NULL),
                                                          keep_alive_pool_(NULL),
                                                          stop_(false),
                                                          keep_watch_pool_(NULL),
                                                          session_timeout_ctx_(NULL) {
    Init(members);
}
void InsSDK::Init(const std::vector<std::string>& members) {
    rpc_client_ = NULL;
    mu_ = NULL;
    keep_alive_pool_ = NULL;
    stop_ = false;
    keep_watch_pool_ = NULL;
    handle_session_timeout_ = NULL;
    session_timeout_ctx_ = NULL;
    watch_task_id_ = 0;
    last_succ_alive_timestamp_ = ins_common::timer::get_micros();
    if (members.size() < 1) {
        LOG(FATAL, "invalid cluster size");
        abort();
    }
    rpc_client_ = new galaxy::RpcClient();
    mu_ = new Mutex();
    logged_uuid_ = "";
    std::copy(members.begin(), members.end(), std::back_inserter(members_));
    keep_alive_pool_ = new ins_common::ThreadPool();
    keep_watch_pool_ = new ins_common::ThreadPool();
    is_keep_alive_bg_ = false;
    MakeSessionID();
}

void InsSDK::MakeSessionID () {
    MutexLock lock(mu_);
    std::string hostname = "";
    struct utsname buf;
    if (0 != uname(&buf)) {
        *buf.nodename = '\0';
    }
    hostname = buf.nodename;
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    std::stringstream sm_uuid;
    sm_uuid << uuid;
    session_id_ = hostname + "#" + sm_uuid.str();
}

InsSDK::~InsSDK() {
    {
        MutexLock lock(mu_);
        stop_ = true;
        keep_alive_pool_->Stop(true);
        keep_watch_pool_->Stop(true);
    }
    delete rpc_client_;
    delete mu_;
    delete keep_alive_pool_;
    delete keep_watch_pool_;
}

void InsSDK::PrepareServerList(std::vector<std::string>& server_list) {
    MutexLock lock(mu_);
    if (!leader_id_.empty()) {
        server_list.push_back(leader_id_);
    }
    std::copy(members_.begin(), members_.end(),
              std::back_inserter(server_list) );
}

bool InsSDK::ShowCluster(std::vector<ClusterNodeInfo>* cluster_info) {
    assert(cluster_info);
    std::vector<std::string>::iterator it;
    for(it = members_.begin(); it != members_.end(); it++) {
        ClusterNodeInfo  node_info;
        node_info.server_id = *it;
        galaxy::ins::InsNode_Stub* stub;
        rpc_client_->GetStub(*it, &stub);
        boost::scoped_ptr<galaxy::ins::InsNode_Stub> stub_guard(stub);
        ::galaxy::ins::ShowStatusRequest request;
        ::galaxy::ins::ShowStatusResponse response;
        bool ok = rpc_client_->SendRequest(stub, &InsNode_Stub::ShowStatus, 
                                          &request, &response, 2, 1);
        if (!ok) {
            node_info.status = kOffline;
            node_info.term = -1;
            node_info.last_log_index = -1;
            node_info.last_log_term = -1;
            node_info.commit_index = -1;
            node_info.last_applied = -1;
        } else {
            node_info.status = response.status();
            node_info.term = response.term();
            node_info.last_log_index = response.last_log_index();
            node_info.last_log_term = response.last_log_term();
            node_info.commit_index = response.commit_index();
            node_info.last_applied = response.last_applied();
        }
        cluster_info->push_back(node_info);
    }
    return true;
}

std::string InsSDK::StatusToString(int32_t status) {
    switch (status) {
        case kLeader:
            return "Leader  ";
            break;
        case kCandidate:
            return "Candidate";
            break;
        case kFollower:
            return "Follower";
            break;
        case kOffline:
            return "Offline";
            break;
    }
    return "UnKnown";
}


bool InsSDK::Put(const std::string& key, const std::string& value, SDKError* error) {
    std::vector<std::string> server_list;
    PrepareServerList(server_list);
    std::vector<std::string>::const_iterator it ;
    for (it = server_list.begin(); it != server_list.end(); it++){
        std::string server_id = *it;
        LOG(DEBUG, "rpc to %s", server_id.c_str());
        galaxy::ins::InsNode_Stub *stub, *stub2;
        rpc_client_->GetStub(server_id, &stub);
        boost::scoped_ptr<galaxy::ins::InsNode_Stub> stub_guard(stub);
        galaxy::ins::PutRequest request;
        galaxy::ins::PutResponse response;
        {
            MutexLock lock(mu_);
            request.set_user(logged_uuid_);
        }
        request.set_key(key);
        request.set_value(value);
        bool ok = rpc_client_->SendRequest(stub, &InsNode_Stub::Put,
                                          &request, &response, 2, 1);
        if (!ok) {
            LOG(FATAL, "faild to rpc %s", server_id.c_str());
            continue;
        }

        if (response.success()) {
            {
                MutexLock lock(mu_);
                leader_id_ = server_id;
            }
            *error = kOK;
            return true;
        } else {
            if (!response.leader_id().empty()) {
                server_id = response.leader_id();
                LOG(DEBUG, "redirect to leader :%s", server_id.c_str());
                rpc_client_->GetStub(server_id, &stub2);
                boost::scoped_ptr<galaxy::ins::InsNode_Stub> stub_guard2(stub2);
                ok = rpc_client_->SendRequest(stub2, &InsNode_Stub::Put,
                                             &request, &response, 2, 1);
                if (ok && response.success()) {
                    {
                        MutexLock lock(mu_);
                        leader_id_ = server_id;
                    }
                    *error = kOK;
                    return true;
                }
            }
        }
        ThisThread::Sleep(1000);
    }
    *error = kClusterDown;
    return false;
}

bool InsSDK::Get(const std::string& key, std::string* value,
                 SDKError* error) {
    std::vector<std::string> server_list;
    PrepareServerList(server_list);
    std::vector<std::string>::const_iterator it ;
    for (it = server_list.begin(); it != server_list.end(); it++){
        std::string server_id = *it;
        LOG(DEBUG, "rpc to %s", server_id.c_str());
        galaxy::ins::InsNode_Stub *stub, *stub2;
        rpc_client_->GetStub(server_id, &stub);
        boost::scoped_ptr<galaxy::ins::InsNode_Stub> stub_guard(stub);
        galaxy::ins::GetRequest request;
        galaxy::ins::GetResponse response;
        {
            MutexLock lock(mu_);
            request.set_user(logged_uuid_);
        }
        request.set_key(key);
        bool ok = rpc_client_->SendRequest(stub, &InsNode_Stub::Get,
                                          &request, &response, 2, 1);
        if (!ok) {
            LOG(FATAL, "faild to rpc %s", server_id.c_str());
            continue;
        }

        if (response.success()) {
            *value = response.value();
            if (response.hit()) {
                *error = kOK;
            } else {
                *error = kNoSuchKey;
            }
            {
                MutexLock lock(mu_);
                leader_id_ = server_id;
            }
            return true;
        } else {
            if (!response.leader_id().empty()) {
                server_id = response.leader_id();
                LOG(DEBUG, "redirect to leader :%s", server_id.c_str());
                rpc_client_->GetStub(server_id, &stub2);
                boost::scoped_ptr<galaxy::ins::InsNode_Stub> stub_guard2(stub2);
                ok = rpc_client_->SendRequest(stub2, &InsNode_Stub::Get,
                                             &request, &response, 2, 1);
                if (ok && response.success()) {
                    {
                        MutexLock lock(mu_);
                        leader_id_ = server_id;
                    }
                    *error = kOK;
                    if (response.hit()) {
                        *error = kOK;
                        *value = response.value();
                    } else {
                        *error = kNoSuchKey;
                    }
                    return true;
                }
            }
        }
        ThisThread::Sleep(1000);
    }
    *error = kClusterDown;
    return false;
}

bool InsSDK::ScanOnce(const std::string& start_key,
                      const std::string& end_key,
                      std::vector<KVPair>* buffer,
                      SDKError* error) {
    assert(buffer);
    std::string value;
    if (!Get(start_key, &value, error)) { //avoid network partition problem
        LOG(FATAL, "the leader may be unavilable");
        return false;
    }
    std::vector<std::string> server_list;
    PrepareServerList(server_list);
    std::vector<std::string>::const_iterator it ;
    for (it = server_list.begin(); it != server_list.end(); it++){
        std::string server_id = *it;
        LOG(DEBUG, "rpc to %s", server_id.c_str());
        galaxy::ins::InsNode_Stub *stub, *stub2;
        rpc_client_->GetStub(server_id, &stub);
        boost::scoped_ptr<galaxy::ins::InsNode_Stub> stub_guard(stub);
        galaxy::ins::ScanRequest request;
        galaxy::ins::ScanResponse response;
        {
            MutexLock lock(mu_);
            request.set_user(logged_uuid_);
        }
        request.set_start_key(start_key);
        request.set_end_key(end_key);
        request.set_size_limit(500);
        bool ok = rpc_client_->SendRequest(stub, &InsNode_Stub::Scan,
                                           &request, &response, 5, 1);
        if (!ok) {
            LOG(FATAL, "faild to rpc %s", server_id.c_str());
            continue;
        }

        if (response.success()) {
            *error = kOK;
            for(int i = 0; i < response.items_size(); i++) {
                KVPair kv_pair;
                kv_pair.key = response.items(i).key();
                kv_pair.value = response.items(i).value();
                buffer->push_back(kv_pair);
            }
            {
                MutexLock lock(mu_);
                leader_id_ = server_id;
            }
            return true;
        } else {
            if (!response.leader_id().empty()) {
                server_id = response.leader_id();
                LOG(DEBUG, "redirect to leader :%s", server_id.c_str());
                rpc_client_->GetStub(server_id, &stub2);
                boost::scoped_ptr<galaxy::ins::InsNode_Stub> stub_guard2(stub2);
                ok = rpc_client_->SendRequest(stub2, &InsNode_Stub::Scan,
                                              &request, &response, 5, 1);
                if (ok && response.success()) {
                    {
                        MutexLock lock(mu_);
                        leader_id_ = server_id;
                    }
                    *error = kOK;
                    for(int i = 0; i < response.items_size(); i++) {
                        KVPair kv_pair;
                        kv_pair.key = response.items(i).key();
                        kv_pair.value = response.items(i).value();
                        buffer->push_back(kv_pair);
                    }       
                    return true;
                }
            }
        }
        ThisThread::Sleep(1000);
    }
    *error = kClusterDown;
    return false;
}

bool InsSDK::Delete(const std::string& key, SDKError* error) {
    std::vector<std::string> server_list;
    PrepareServerList(server_list);
    std::vector<std::string>::const_iterator it ;
    for (it = server_list.begin(); it != server_list.end(); it++){
        std::string server_id = *it;
        LOG(DEBUG, "rpc to %s", server_id.c_str());
        galaxy::ins::InsNode_Stub *stub, *stub2;
        rpc_client_->GetStub(server_id, &stub);
        galaxy::ins::DelRequest request;
        galaxy::ins::DelResponse response;
        {
            MutexLock lock(mu_);
            request.set_user(logged_uuid_);
        }
        request.set_key(key);
        bool ok = rpc_client_->SendRequest(stub, &InsNode_Stub::Delete,
                                          &request, &response, 2, 1);
        if (!ok) {
            LOG(FATAL, "faild to rpc %s", server_id.c_str());
            continue;
        }

        if (response.success()) {
            {
                MutexLock lock(mu_);
                leader_id_ = server_id;
            }
            *error = kOK;
            return true;
        } else {
            if (!response.leader_id().empty()) {
                server_id = response.leader_id();
                LOG(DEBUG, "redirect to leader :%s", server_id.c_str());
                rpc_client_->GetStub(server_id, &stub2);
                boost::scoped_ptr<galaxy::ins::InsNode_Stub> stub_guard2(stub2);
                ok = rpc_client_->SendRequest(stub2, &InsNode_Stub::Delete,
                                             &request, &response, 2, 1);
                if (ok && response.success()) {
                    {
                        MutexLock lock(mu_);
                        leader_id_ = server_id;
                    }
                    *error = kOK;
                    return true;
                }
            } 
        }
        ThisThread::Sleep(1000);
    }
    *error = kClusterDown;
    return false;
}

bool InsSDK::Watch(const std::string& key, 
                   WatchCallback user_callback, 
                   void* context,
                   SDKError* error) {
    std::string old_value;
    Get(key, &old_value, error);
    if (*error != kOK && *error != kNoSuchKey) {
        LOG(FATAL, "faild to issue a watch: %s", key.c_str());
        return false;
    }
    bool key_exist = true;
    if (*error == kNoSuchKey) {
        key_exist = false;
    }
    {
        MutexLock lock(mu_);
        watch_keys_.insert(key);
        watch_cbs_[key] = user_callback;
        watch_ctx_[key] = context;
        int64_t watch_id = (++watch_task_id_);
        pending_watches_.insert(watch_id);
        if (!is_keep_alive_bg_) {
            keep_alive_pool_->AddTask(
                boost::bind(&InsSDK::KeepAliveTask, this)
            );
            is_keep_alive_bg_ = true;
        }
        keep_watch_pool_->AddTask(
            boost::bind(&InsSDK::KeepWatchTask, this, key,
                        old_value, key_exist, session_id_, watch_id)
        );
    }
    *error = kOK;
    return true;
}

void InsSDK::KeepAliveTask() {
    std::set<std::string> my_locks;
    {
        MutexLock lock(mu_);
        if (stop_) {
            return;
        }
        std::set<std::string>::iterator it;
        for (it = lock_keys_.begin(); it != lock_keys_.end(); it++) {
            my_locks.insert(*it);
        }
    }
    std::vector<std::string> server_list;
    PrepareServerList(server_list);
    std::vector<std::string>::const_iterator it ;
    for (it = server_list.begin(); it != server_list.end(); it++){
        std::string server_id = *it;
        LOG(DEBUG, "rpc to %s", server_id.c_str());
        galaxy::ins::InsNode_Stub *stub, *stub2;
        rpc_client_->GetStub(server_id, &stub);
        boost::scoped_ptr<galaxy::ins::InsNode_Stub> stub_guard(stub);
        galaxy::ins::KeepAliveRequest request;
        galaxy::ins::KeepAliveResponse response;
        request.set_session_id(GetSessionID());
        request.set_uuid(logged_uuid_);
        std::set<std::string>::iterator si;
        for (si = my_locks.begin(); si != my_locks.end(); si++) {
            std::string* lock_key = request.add_locks();
            *lock_key = *si;
        }
        bool ok = rpc_client_->SendRequest(stub, &InsNode_Stub::KeepAlive,
                                           &request, &response, 2, 1);
        if (!ok) {
            LOG(FATAL, "faild to rpc %s", server_id.c_str());
            continue;
        }

        if (response.success()) {
            {
                MutexLock lock(mu_);
                leader_id_ = server_id;
                last_succ_alive_timestamp_ = ins_common::timer::get_micros();
            }
            break;
        } else {
            if (!response.leader_id().empty()) {
                server_id = response.leader_id();
                LOG(DEBUG, "redirect to leader :%s", server_id.c_str());
                rpc_client_->GetStub(server_id, &stub2);
                boost::scoped_ptr<galaxy::ins::InsNode_Stub> stub_guard2(stub2);
                ok = rpc_client_->SendRequest(stub2, &InsNode_Stub::KeepAlive,
                                              &request, &response, 2, 1);
                if (ok && response.success()) {
                    {
                        MutexLock lock(mu_);
                        leader_id_ = server_id;
                        last_succ_alive_timestamp_ = ins_common::timer::get_micros();
                    }
                    break;
                }
            }
        }
    } // end of for
    
    bool session_expire = false;
    {
        MutexLock lock(mu_);
        int64_t now = ins_common::timer::get_micros();
        int64_t span = now - last_succ_alive_timestamp_;
        if (span > 30000000) {// 30 seconds
            if (handle_session_timeout_) { //session timeout callback
                LOG(INFO, "call user callback of session timeout: %x",
                    handle_session_timeout_);
                handle_session_timeout_(session_timeout_ctx_);
            }
            session_expire = true;
        }
    }
    if (session_expire) {
        MakeSessionID();
        LOG(INFO, "create a new session: %s", GetSessionID().c_str());
        {
            MutexLock lock(mu_);
            lock_keys_.clear();
        }
    }
    
    keep_alive_pool_->DelayTask(2000,
        boost::bind(&InsSDK::KeepAliveTask, this)
    );
}

void InsSDK::KeepWatchCallback(const galaxy::ins::WatchRequest* request,
                               galaxy::ins::WatchResponse* response,
                               bool failed, int /*error*/,
                               std::string server_id,
                               int64_t watch_id) {
    boost::scoped_ptr<const galaxy::ins::WatchRequest> request_ptr(request);
    boost::scoped_ptr<galaxy::ins::WatchResponse> response_ptr(response);
    {
        MutexLock lock(mu_);
        if (stop_) {
            return;
        }
    }
    if (!failed && response_ptr->success()) {
        WatchCallback cb = NULL;
        void * cb_ctx = NULL;
        {
            MutexLock lock(mu_);
            leader_id_ = server_id;
            cb = watch_cbs_[response_ptr->watch_key()];
            cb_ctx = watch_ctx_[response_ptr->watch_key()];
        }
        if (cb) {
            {
                MutexLock lock(mu_);
                watch_keys_.erase(response->watch_key());
                watch_cbs_.erase(response->watch_key());
                watch_ctx_.erase(response->watch_key());
                pending_watches_.erase(watch_id);
                LOG(INFO, "watch #%ld trigger", watch_id);
            }
            WatchParam param;
            param.key = response_ptr->key();
            param.value = response_ptr->value();
            param.deleted = response_ptr->deleted();
            param.context = cb_ctx;
            cb(param, kOK);
        }
        return;
    } else if (!failed && !response_ptr->leader_id().empty()) {
        server_id = response_ptr->leader_id();
    } else {
        std::vector<std::string> server_list;
        PrepareServerList(server_list);
        int s_no = (int32_t) (server_list.size() * rand()/(RAND_MAX+1.0));
        server_id = server_list[s_no];
    }
    if (!response_ptr->canceled()) { //retry, if not cancel
        if (request->session_id() != GetSessionID()) {
            LOG(INFO, "callback, no retry on expired watch");
            return;        
        }
        {
            MutexLock lock(mu_);
            if (watch_keys_.find(request->key()) == watch_keys_.end()) {
                LOG(INFO, "watcher has been triggered");
                return;
            }
        }
        LOG(INFO, "watch again to %s", server_id.c_str());
        galaxy::ins::InsNode_Stub *stub;
        rpc_client_->GetStub(server_id, &stub);
        boost::scoped_ptr<galaxy::ins::InsNode_Stub> stub_guard(stub);
        galaxy::ins::WatchRequest* req = new galaxy::ins::WatchRequest();
        galaxy::ins::WatchResponse* rsps = new galaxy::ins::WatchResponse();
        req->CopyFrom(*request);
        boost::function< void (const galaxy::ins::WatchRequest*,
                               galaxy::ins::WatchResponse*, 
                               bool, int) > callback;
        callback = boost::bind(&InsSDK::KeepWatchCallback, this, _1, _2, _3, _4, 
                               server_id, watch_id);
        rpc_client_->AsyncRequest(stub, &InsNode_Stub::Watch,
                                  req, rsps, callback, 
                                  FLAGS_ins_watch_timeout, 1); //120s timeout
    } else {
        LOG(INFO, "the previous watch is canceled");
    }
}

void InsSDK::BackupWatchTask(const std::string& key, int64_t watch_id) {
    std::string old_value;
    SDKError error;
    Get(key, &old_value, &error);
    if (error != kOK && error != kNoSuchKey) {
        LOG(FATAL, "faild to issue a backup watch: %s", key.c_str());
        return;
    }
    bool key_exist = true;
    if (error == kNoSuchKey) {
        key_exist = false;
    }
    KeepWatchTask(key, old_value, key_exist, GetSessionID(), watch_id);
}

void InsSDK::KeepWatchTask(const std::string& key, 
                           const std::string& old_value,
                           bool key_exist,
                           std::string session_id,
                           int64_t watch_id) {
    {
        MutexLock lock(mu_);
        if (stop_) {
            return;
        }
        if (pending_watches_.find(watch_id) == pending_watches_.end()) {
            LOG(INFO, "expired watch id :%ld", watch_id);
            return;
        }
    }

    if (session_id != GetSessionID()) {
        LOG(INFO, "expired watch on %s", key.c_str());
        return;
    }

    keep_watch_pool_->DelayTask(FLAGS_ins_backup_watch_timeout * 1000, //ms
        boost::bind(&InsSDK::BackupWatchTask, this, key, watch_id)
    );
    std::vector<std::string> server_list;
    PrepareServerList(server_list);
    int s_no = (int32_t) (server_list.size() * rand()/(RAND_MAX+1.0));
    std::string server_id = server_list[s_no];
    LOG(INFO, "watch to %s", server_id.c_str());
    galaxy::ins::InsNode_Stub *stub;
    rpc_client_->GetStub(server_id, &stub);
    boost::scoped_ptr<galaxy::ins::InsNode_Stub> stub_guard(stub);
    galaxy::ins::WatchRequest* request = new galaxy::ins::WatchRequest();
    galaxy::ins::WatchResponse* response = new galaxy::ins::WatchResponse();
    {
        MutexLock lock(mu_);
        request->set_user(logged_uuid_);
    }
    request->set_session_id(GetSessionID());
    request->set_key(key);
    request->set_old_value(old_value);
    request->set_key_exist(key_exist);
    boost::function< void (const galaxy::ins::WatchRequest*,
                           galaxy::ins::WatchResponse*, 
                           bool, int) > callback;
    callback = boost::bind(&InsSDK::KeepWatchCallback, this, _1, _2, _3, _4, 
                           server_id, watch_id);
    rpc_client_->AsyncRequest(stub, &InsNode_Stub::Watch,
                              request, response, callback, 
                              FLAGS_ins_watch_timeout, 1); 
}

bool InsSDK::Lock(const std::string& key, SDKError* error) {
    {
        MutexLock lock(mu_);
        if (!is_keep_alive_bg_) {
            keep_alive_pool_->AddTask(
                boost::bind(&InsSDK::KeepAliveTask, this)
            );
            is_keep_alive_bg_ = true;
        }
    }
    LOG(INFO, "try lock on :%s", key.c_str());
    while (!TryLock(key, error)) {
        LOG(INFO, "try lock again on :%s", key.c_str());
        ThisThread::Sleep(1000);
        {
            MutexLock lock(mu_);
            if (stop_) {
                break;
            }
        }
    }
    if (*error != kOK) {
        return false;
    }
    {
        MutexLock lock(mu_);
        lock_keys_.insert(key);
    }
    return true;
}

bool InsSDK::TryLock(const std::string& key, SDKError *error) {
    {
        MutexLock lock(mu_);
        if (!is_keep_alive_bg_) {
            keep_alive_pool_->AddTask(
                    boost::bind(&InsSDK::KeepAliveTask, this)
                    );
            is_keep_alive_bg_ = true;
        }
    }
    std::vector<std::string> server_list;
    PrepareServerList(server_list);
    std::vector<std::string>::const_iterator it ;
    for (it = server_list.begin(); it != server_list.end(); it++){
        std::string server_id = *it;
        LOG(DEBUG, "rpc to %s", server_id.c_str());
        galaxy::ins::InsNode_Stub *stub, *stub2;
        rpc_client_->GetStub(server_id, &stub);
        boost::scoped_ptr<galaxy::ins::InsNode_Stub> stub_guard(stub);
        galaxy::ins::LockRequest request;
        galaxy::ins::LockResponse response;
        {
            MutexLock lock(mu_);
            request.set_user(logged_uuid_);
        }
        request.set_key(key);
        request.set_session_id(GetSessionID());
        bool ok = rpc_client_->SendRequest(stub, &InsNode_Stub::Lock,
                                           &request, &response, 2, 1);
        if (!ok) {
            LOG(FATAL, "faild to rpc %s", server_id.c_str());
            continue;
        }

        if (response.success()) {
            {
                MutexLock lock(mu_);
                leader_id_ = server_id;
            }
            *error = kOK;
            return true;
        } else {
            if (!response.leader_id().empty()) {
                server_id = response.leader_id();
                LOG(DEBUG, "redirect to leader :%s", server_id.c_str());
                rpc_client_->GetStub(server_id, &stub2);
                boost::scoped_ptr<galaxy::ins::InsNode_Stub> stub_guard2(stub2);
                ok = rpc_client_->SendRequest(stub2, &InsNode_Stub::Lock,
                                              &request, &response, 2, 1);
                if (ok && response.success()) {
                    {
                        MutexLock lock(mu_);
                        leader_id_ = server_id;
                    }
                    *error = kOK;
                    return true;
                }
            }
        }
    }
    *error = kLockFail;
    return false;
}

bool InsSDK::UnLock(const std::string& key, SDKError* error) {
    std::vector<std::string> server_list;
    PrepareServerList(server_list);
    std::vector<std::string>::const_iterator it ;
    for (it = server_list.begin(); it != server_list.end(); it++){
        std::string server_id = *it;
        LOG(DEBUG, "rpc to %s", server_id.c_str());
        galaxy::ins::InsNode_Stub *stub, *stub2;
        rpc_client_->GetStub(server_id, &stub);
        boost::scoped_ptr<galaxy::ins::InsNode_Stub> stub_guard(stub);
        galaxy::ins::UnLockRequest request;
        galaxy::ins::UnLockResponse response;
        {
            MutexLock lock(mu_);
            request.set_user(logged_uuid_);
        }
        request.set_key(key);
        request.set_session_id(GetSessionID());
        bool ok = rpc_client_->SendRequest(stub, &InsNode_Stub::UnLock,
                                          &request, &response, 2, 1);
        if (!ok) {
            LOG(FATAL, "faild to rpc %s", server_id.c_str());
            continue;
        }

        if (response.success()) {
            {
                MutexLock lock(mu_);
                leader_id_ = server_id;
                lock_keys_.erase(key);
            }
            *error = kOK;
            return true;
        } else {
            if (!response.leader_id().empty()) {
                server_id = response.leader_id();
                LOG(DEBUG, "redirect to leader :%s", server_id.c_str());
                rpc_client_->GetStub(server_id, &stub2);
                boost::scoped_ptr<galaxy::ins::InsNode_Stub> stub_guard2(stub2);
                ok = rpc_client_->SendRequest(stub2, &InsNode_Stub::UnLock,
                                              &request, &response, 2, 1);
                if (ok && response.success()) {
                    {
                        MutexLock lock(mu_);
                        leader_id_ = server_id;
                        lock_keys_.erase(key);
                    }
                    *error = kOK;
                    return true;
                }
            }
        }
        ThisThread::Sleep(1000);
    }
    *error = kClusterDown;
    return false;
}

std::string InsSDK::HashPassword(const std::string& password) {
    boost::uuids::detail::sha1 sha;
    sha.process_bytes(password.c_str(), password.size());
    unsigned int digest[5];
    sha.get_digest(digest);
    std::stringstream ss;
    for (int i = 0; i < 5; ++i) {
        ss << std::hex << digest[i];
    }
    return ss.str();
}

bool InsSDK::Login(const std::string& username,
                   const std::string& password,
                   SDKError* error) {
    {
        MutexLock lock(mu_);
        if (!logged_uuid_.empty()) {
            *error = kUserLogged;
            return true;
        }
        if (!is_keep_alive_bg_) {
            keep_alive_pool_->AddTask(
                boost::bind(&InsSDK::KeepAliveTask, this)
            );
            is_keep_alive_bg_ = true;
        }
    }
    std::vector<std::string> server_list;
    PrepareServerList(server_list);
    std::vector<std::string>::const_iterator it;
    for (it = server_list.begin(); it != server_list.end(); ++it) {
        std::string server_id = *it;
        LOG(DEBUG, "rpc to %s", server_id.c_str());
        galaxy::ins::InsNode_Stub *stub, *stub2;
        rpc_client_->GetStub(server_id, &stub);
        boost::scoped_ptr<galaxy::ins::InsNode_Stub> stub_guard(stub);
        galaxy::ins::LoginRequest request;
        galaxy::ins::LoginResponse response;
        request.set_username(username);
        request.set_passwd(HashPassword(password));
        bool ok = rpc_client_->SendRequest(stub, &InsNode_Stub::Login,
                                           &request, &response, 2, 1);
        if (!ok) {
            LOG(FATAL, "failed to rpc %s", server_id.c_str());
            continue;
        }

        if (response.status() != galaxy::ins::kError) {
            {
                MutexLock lock(mu_);
                leader_id_ = server_id;
            }
            switch(response.status()) {
            case galaxy::ins::kOk: *error = kOK; logged_uuid_ = response.uuid(); return true;
            case galaxy::ins::kUnknownUser: *error = kUnknownUser; return true;
            case galaxy::ins::kUserExists: *error = kUserLogged; return true;
            case galaxy::ins::kPasswordError: *error = kPasswordError; return true;
            default: break; // pass
            }
        } else {
            if (!response.leader_id().empty()) {
                server_id = response.leader_id();
                LOG(DEBUG, "redirect to leader :%s", server_id.c_str());
                rpc_client_->GetStub(server_id, &stub2);
                boost::scoped_ptr<galaxy::ins::InsNode_Stub> stub_guard2(stub2);
                ok = rpc_client_->SendRequest(stub2, &InsNode_Stub::Login,
                                              &request, &response, 2, 1);
                if (response.status() != galaxy::ins::kError) {
                    {
                        MutexLock lock(mu_);
                        leader_id_ = server_id;
                    }
                    switch(response.status()) {
                    case galaxy::ins::kOk: *error = kOK;
                                           logged_uuid_ = response.uuid();
                                           return true;
                    case galaxy::ins::kUnknownUser: *error = kUnknownUser; return true;
                    case galaxy::ins::kUserExists: *error = kUserLogged; return true;
                    case galaxy::ins::kPasswordError: *error = kPasswordError; return true;
                    default: break; // pass
                    }
                }
            }
        }
    }
    *error = kClusterDown;
    return false;
}

bool InsSDK::Logout(SDKError* error) {
    {
        MutexLock lock(mu_);
        if (logged_uuid_.empty()) {
            *error = kUnknownUser;
            return true;
        }
    }
    std::vector<std::string> server_list;
    PrepareServerList(server_list);
    std::vector<std::string>::const_iterator it;
    for (it = server_list.begin(); it != server_list.end(); ++it) {
        std::string server_id = *it;
        LOG(DEBUG, "rpc to %s", server_id.c_str());
        galaxy::ins::InsNode_Stub *stub, *stub2;
        rpc_client_->GetStub(server_id, &stub);
        boost::scoped_ptr<galaxy::ins::InsNode_Stub> stub_guard(stub);
        galaxy::ins::LogoutRequest request;
        galaxy::ins::LogoutResponse response;
        request.set_uuid(logged_uuid_);
        bool ok = rpc_client_->SendRequest(stub, &InsNode_Stub::Logout,
                                           &request, &response, 2, 1);
        if (!ok) {
            LOG(FATAL, "failed to rpc %s", server_id.c_str());
            continue;
        }

        if (response.status() != galaxy::ins::kError) {
            {
                MutexLock lock(mu_);
                leader_id_ = server_id;
            }
            switch(response.status()) {
            case galaxy::ins::kOk: *error = kOK; logged_uuid_ = ""; return true;
            // Maybe have logged out due to time out
            case galaxy::ins::kUnknownUser: *error = kUnknownUser; logged_uuid_ = ""; return true;
            default: break; // pass
            }
        } else {
            if (!response.leader_id().empty()) {
                server_id = response.leader_id();
                LOG(DEBUG, "redirect to leader :%s", server_id.c_str());
                rpc_client_->GetStub(server_id, &stub2);
                boost::scoped_ptr<galaxy::ins::InsNode_Stub> stub_guard2(stub2);
                ok = rpc_client_->SendRequest(stub2, &InsNode_Stub::Logout,
                                              &request, &response, 2, 1);
                if (response.status() != galaxy::ins::kError) {
                    {
                        MutexLock lock(mu_);
                        leader_id_ = server_id;
                    }
                    switch(response.status()) {
                    case galaxy::ins::kOk: *error = kOK; logged_uuid_ = ""; return true;
                    case galaxy::ins::kUnknownUser: *error = kUnknownUser;
                                                    logged_uuid_ = "";
                                                    return true;
                    default: break; // pass
                    }
                }
            }
        }
    }
    *error = kClusterDown;
    return false;
}

bool InsSDK::Register(const std::string& username,
                      const std::string& password,
                      SDKError* error) {
    std::vector<std::string> server_list;
    PrepareServerList(server_list);
    std::vector<std::string>::const_iterator it;
    for (it = server_list.begin(); it != server_list.end(); ++it) {
        std::string server_id = *it;
        LOG(DEBUG, "rpc to %s", server_id.c_str());
        galaxy::ins::InsNode_Stub *stub, *stub2;
        rpc_client_->GetStub(server_id, &stub);
        boost::scoped_ptr<galaxy::ins::InsNode_Stub> stub_guard(stub);
        galaxy::ins::RegisterRequest request;
        galaxy::ins::RegisterResponse response;
        request.set_username(username);
        request.set_passwd(HashPassword(password));
        bool ok = rpc_client_->SendRequest(stub, &InsNode_Stub::Register,
                                           &request, &response, 2, 1);
        if (!ok) {
            LOG(FATAL, "failed to rpc %s", server_id.c_str());
            continue;
        }

        if (response.status() != galaxy::ins::kError) {
            {
                MutexLock lock(mu_);
                leader_id_ = server_id;
            }
            switch(response.status()) {
            case galaxy::ins::kOk: *error = kOK; return true;
            case galaxy::ins::kUserExists: *error = kUserExists; return true;
            default: break; // pass
            }
        } else {
            if (!response.leader_id().empty()) {
                server_id = response.leader_id();
                LOG(DEBUG, "redirect to leader :%s", server_id.c_str());
                rpc_client_->GetStub(server_id, &stub2);
                boost::scoped_ptr<galaxy::ins::InsNode_Stub> stub_guard2(stub2);
                ok = rpc_client_->SendRequest(stub2, &InsNode_Stub::Register,
                                              &request, &response, 2, 1);
                if (response.status() != galaxy::ins::kError) {
                    {
                        MutexLock lock(mu_);
                        leader_id_ = server_id;
                    }
                    switch(response.status()) {
                    case galaxy::ins::kOk: *error = kOK; return true;
                    case galaxy::ins::kUserExists: *error = kUserExists; return true;
                    default: break; // pass
                    }
                }
            }
        }
    }
    *error = kClusterDown;
    return false;
}

bool InsSDK::CleanBinlog(const std::string& server_id,
                         int64_t end_index, 
                         SDKError* error) {
    galaxy::ins::InsNode_Stub *stub;
    rpc_client_->GetStub(server_id, &stub);
    boost::scoped_ptr<galaxy::ins::InsNode_Stub> stub_guard(stub);
    galaxy::ins::CleanBinlogRequest request;
    galaxy::ins::CleanBinlogResponse response;
    request.set_end_index(end_index);
    bool ok = rpc_client_->SendRequest(stub, &InsNode_Stub::CleanBinlog,
                                       &request, &response, 2, 1);
    if (!ok) {
        *error = kTimeout;
        return false;
    }
    if (ok && !response.success()) {
        *error = kCleanBinlogFail;
        LOG(FATAL, "remove binlog at %ld is unsafe", end_index);
        return false;
    }
    return true;
}

std::string InsSDK::GetSessionID() {
    MutexLock lock(mu_);
    return session_id_;
}

std::string InsSDK::GetCurrentUserID() {
    MutexLock lock(mu_);
    return logged_uuid_;
}

bool InsSDK::IsLoggedIn() {
    MutexLock lock(mu_);
    return !logged_uuid_.empty();
}

void InsSDK::RegisterSessionTimeout(void (*handle_session_timeout)(void *), void* ctx) {
    assert(handle_session_timeout);
    MutexLock lock(mu_);
    handle_session_timeout_ = handle_session_timeout;
    session_timeout_ctx_ = ctx;
    LOG(INFO, "session timeout handler: %x", handle_session_timeout_);
}

ScanResult::ScanResult(InsSDK* sdk) : offset_(0),
                                      sdk_(sdk),
                                      error_(kOK) {

}

ScanResult* InsSDK::Scan(const std::string& start_key, 
                         const std::string& end_key) {
    ScanResult* result =  new ScanResult(this);
    result->Init(start_key, end_key);
    return result;
}

void ScanResult::Init(const std::string& start_key,
                      const std::string& end_key) {
    assert(sdk_);
    end_key_ =  end_key;
    sdk_->ScanOnce(start_key, end_key, &buffer_, &error_);
    offset_ = 0;
}

bool ScanResult::Done() {
    return buffer_.empty();
}

SDKError ScanResult::Error() {
    return error_;
}

const std::string ScanResult::Key() {
    assert(offset_ < buffer_.size());
    return buffer_[offset_].key;
}

const std::string ScanResult::Value() {
    assert(offset_ < buffer_.size());
    return buffer_[offset_].value;
}

void ScanResult::Next() {
    offset_ ++ ;
    if (offset_ >= buffer_.size() && !buffer_.empty()) {
        std::string last_key = buffer_[buffer_.size()-1].key;
        std::vector<KVPair> empty_v;
        buffer_.swap(empty_v);
        last_key.append(1,'\0');
        sdk_->ScanOnce(last_key, end_key_, &buffer_, &error_); 
        offset_ = 0;      
    }
}

} //namespace sdk
} //namespace ins
} //namespace galaxy

