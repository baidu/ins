#include "ins_sdk.h"

#include <assert.h>
#include <algorithm>
#include <iterator>
#include <vector>
#include <boost/algorithm/string.hpp>
#include <boost/scoped_ptr.hpp>
#include <gflags/gflags.h>
#include "common/asm_atomic.h"
#include "common/mutex.h"
#include "common/this_thread.h"
#include "common/thread_pool.h"
#include "rpc/rpc_client.h"
#include "proto/ins_node.pb.h"

DECLARE_string(cluster_members);

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
        exit(1);
    }
}

InsSDK::InsSDK(const std::vector<std::string>& members) : rpc_client_(NULL),
                                                          mu_(NULL),
                                                          watch_pool_(NULL) {
    if (members.size() < 1) {
        LOG(FATAL, "invalid cluster size");
        exit(1);
    }
    rpc_client_ = new galaxy::RpcClient();
    mu_ = new Mutex();
    std::copy(members.begin(), members.end(), std::back_inserter(members_));
    watch_pool_ = new common::ThreadPool();
}

InsSDK::~InsSDK() {
    delete rpc_client_;
    delete mu_;
    delete watch_pool_;
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
        } else {
            node_info.status = response.status();
            node_info.term = response.term();
            node_info.last_log_index = response.last_log_index();
            node_info.last_log_term = response.last_log_term();
            node_info.commit_index = response.commit_index();
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
        request.set_key(key);
        request.set_value(value);
        bool ok = rpc_client_->SendRequest(stub, &InsNode_Stub::Put,
                                          &request, &response, 2, 1);
        if (!ok) {
            LOG(FATAL, "faild to rcp %s", server_id.c_str());
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
        request.set_key(key);
        bool ok = rpc_client_->SendRequest(stub, &InsNode_Stub::Get,
                                          &request, &response, 2, 1);
        if (!ok) {
            LOG(FATAL, "faild to rcp %s", server_id.c_str());
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
        request.set_start_key(start_key);
        request.set_end_key(end_key);
        request.set_size_limit(500);
        bool ok = rpc_client_->SendRequest(stub, &InsNode_Stub::Scan,
                                           &request, &response, 5, 1);
        if (!ok) {
            LOG(FATAL, "faild to rcp %s", server_id.c_str());
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
        request.set_key(key);
        bool ok = rpc_client_->SendRequest(stub, &InsNode_Stub::Delete,
                                          &request, &response, 2, 1);
        if (!ok) {
            LOG(FATAL, "faild to rcp %s", server_id.c_str());
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

void InsSDK::WatchTask(const std::string& key,
                       const std::string& old_value,
                       bool has_key,
                       WatchCallback user_callback,
                       void* context) {
    std::string new_value = "";
    SDKError error;
    bool ok = Get(key, &new_value, &error);
    bool watch_again = false;
    if (ok) {
        bool now_has_key = true;
        if (error == kNoSuchKey) {
            now_has_key = false;
        }
        if (has_key != now_has_key || new_value != old_value) {
            user_callback(key, new_value, old_value,
                          now_has_key, context); //trigger callback
            return; //dont't watch again
        } else {
            watch_again = true;
        }
    }  
    if (!ok || watch_again) {
        watch_pool_->DelayTask(2000, 
            boost::bind(&InsSDK::WatchTask, this,
                        key, old_value, has_key,
                        user_callback, context)
        );
    }
}

bool InsSDK::Watch(const std::string& key, 
                   WatchCallback user_callback, 
                   void* context,
                   SDKError* error) {
    assert(watch_pool_);
    std::string old_value;
    bool ok = Get(key, &old_value, error);
    if (!ok) {
        return false;
    }
    bool has_key = true;
    if (*error == kNoSuchKey) {
        has_key = false;
    }
    watch_pool_->AddTask(
        boost::bind(&InsSDK::WatchTask, this,
                    key, old_value, has_key,
                    user_callback, context)
    );
    return true;
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

void ScanResult::Init(const std::string& start_key, const std::string& end_key) {
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

