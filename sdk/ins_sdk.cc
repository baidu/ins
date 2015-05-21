#include "ins_sdk.h"

#include <algorithm>
#include <iterator>
#include <vector>
#include <boost/algorithm/string.hpp>
#include <boost/scoped_ptr.hpp>
#include <gflags/gflags.h>
#include "common/asm_atomic.h"
#include "common/mutex.h"
#include "common/this_thread.h"
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
                                                          mu_(NULL) {
    if (members.size() < 1) {
        LOG(FATAL, "invalid cluster size");
        exit(1);
    }
    rpc_client_ = new galaxy::RpcClient();
    mu_ = new Mutex();
    std::copy(members.begin(), members.end(), std::back_inserter(members_));
}

InsSDK::~InsSDK() {
    delete rpc_client_;
    delete mu_;
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
        } else {
            node_info.status = response.status();
            node_info.term = response.term();
            node_info.last_log_index = response.last_log_index();
            node_info.last_log_term = response.last_log_term();
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
        LOG(INFO, "rpc to %s", server_id.c_str());
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
                LOG(INFO, "redirect to leader :%s", server_id.c_str());
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
                 SDKError* error, bool quorum) {
    std::vector<std::string> server_list;
    PrepareServerList(server_list);
    std::vector<std::string>::const_iterator it ;
    for (it = server_list.begin(); it != server_list.end(); it++){
        std::string server_id = *it;
        LOG(INFO, "rpc to %s", server_id.c_str());
        galaxy::ins::InsNode_Stub *stub, *stub2;
        rpc_client_->GetStub(server_id, &stub);
        boost::scoped_ptr<galaxy::ins::InsNode_Stub> stub_guard(stub);
        galaxy::ins::GetRequest request;
        galaxy::ins::GetResponse response;
        request.set_key(key);
        request.set_quorum(quorum);
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
                LOG(INFO, "redirect to leader :%s", server_id.c_str());
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


bool InsSDK::Delete(const std::string& key, SDKError* error) {
    std::vector<std::string> server_list;
    PrepareServerList(server_list);
    std::vector<std::string>::const_iterator it ;
    for (it = server_list.begin(); it != server_list.end(); it++){
        std::string server_id = *it;
        LOG(INFO, "rpc to %s", server_id.c_str());
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
                LOG(INFO, "redirect to leader :%s", server_id.c_str());
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

} //namespace sdk
} //namespace ins
} //namespace galaxy

