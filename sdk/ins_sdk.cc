#include "ins_sdk.h"

#include <algorithm>
#include <iterator>
#include <vector>
#include "common/asm_atomic.h"

namespace galaxy {
namespace ins {
namespace sdk {

InsSDK::InsSDK(const std::vector<std::string>& members) {
    std::copy(members.begin(), members.end(), std::back_inserter(members_));
}

bool InsSDK::ShowCluster(std::vector<ClusterNodeInfo>* cluster_info) {
    assert(cluster_info);
    std::vector<std::string>::iterator it;
    for(it = members_.begin(); it != members_.end(); it++) {
        ClusterNodeInfo  node_info;
        node_info.server_id = *it;
        galaxy::ins::InsNode_Stub* stub;
        rpc_client_.GetStub(*it, &stub);
        ::galaxy::ins::ShowStatusRequest request;
        ::galaxy::ins::ShowStatusResponse response;
        bool ok = rpc_client_.SendRequest(stub, &InsNode_Stub::ShowStatus, 
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

std::string InsSDK::StatusToString(galaxy::ins::NodeStatus status) {
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
    std::vector<std::string>::const_iterator it ;
    for (it = members_.begin(); it != members_.end(); it++){
        std::string server_id = *it;
        LOG(INFO, "rpc to %s", server_id.c_str());
        galaxy::ins::InsNode_Stub* stub;
        rpc_client_.GetStub(server_id, &stub);
        galaxy::ins::PutRequest request;
        galaxy::ins::PutResponse response;
        request.set_key(key);
        request.set_value(value);
        bool ok = rpc_client_.SendRequest(stub, &InsNode_Stub::Put,
                                          &request, &response, 2, 1);
        if (!ok) {
            LOG(FATAL, "faild to rcp %s", server_id.c_str());
            continue;
        }

        if (response.success()) {
            *error = kOK;
            return true;
        } else {
            if (!response.leader_id().empty()) {
                server_id = response.leader_id();
                LOG(INFO, "redirect to leader :%s", server_id.c_str());
                rpc_client_.GetStub(server_id, &stub);
                ok = rpc_client_.SendRequest(stub, &InsNode_Stub::Put,
                                             &request, &response, 2, 1);
                if (ok && response.success()) {
                    *error = kOK;
                    return true;
                }
            }
        }
    }
    *error = kClusterDown;
    return false;
}

bool InsSDK::Get(const std::string& key, std::string* value, SDKError* error) {
    std::vector<std::string>::const_iterator it ;
    for (it = members_.begin(); it != members_.end(); it++){
        std::string server_id = *it;
        LOG(INFO, "rpc to %s", server_id.c_str());
        galaxy::ins::InsNode_Stub* stub;
        rpc_client_.GetStub(server_id, &stub);
        galaxy::ins::GetRequest request;
        galaxy::ins::GetResponse response;
        request.set_key(key);
        bool ok = rpc_client_.SendRequest(stub, &InsNode_Stub::Get,
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
            return true;
        } else {
            if (!response.leader_id().empty()) {
                server_id = response.leader_id();
                LOG(INFO, "redirect to leader :%s", server_id.c_str());
                rpc_client_.GetStub(server_id, &stub);
                ok = rpc_client_.SendRequest(stub, &InsNode_Stub::Get,
                                             &request, &response, 2, 1);
                if (ok && response.success()) {
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
    }
    *error = kClusterDown;
    return false;
}


bool InsSDK::Delete(const std::string& key, SDKError* error) {
    std::vector<std::string>::const_iterator it ;
    for (it = members_.begin(); it != members_.end(); it++){
        std::string server_id = *it;
        LOG(INFO, "rpc to %s", server_id.c_str());
        galaxy::ins::InsNode_Stub* stub;
        rpc_client_.GetStub(server_id, &stub);
        galaxy::ins::DelRequest request;
        galaxy::ins::DelResponse response;
        request.set_key(key);
        bool ok = rpc_client_.SendRequest(stub, &InsNode_Stub::Delete,
                                          &request, &response, 2, 1);
        if (!ok) {
            LOG(FATAL, "faild to rcp %s", server_id.c_str());
            continue;
        }

        if (response.success()) {
            *error = kOK;
            return true;
        } else {
            if (!response.leader_id().empty()) {
                server_id = response.leader_id();
                LOG(INFO, "redirect to leader :%s", server_id.c_str());
                rpc_client_.GetStub(server_id, &stub);
                ok = rpc_client_.SendRequest(stub, &InsNode_Stub::Delete,
                                             &request, &response, 2, 1);
                if (ok && response.success()) {
                    *error = kOK;
                    return true;
                }
            }
        }
    }
    *error = kClusterDown;
    return false;
}

} //namespace sdk
} //namespace ins
} //namespace galaxy

