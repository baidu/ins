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


/*bool InsSDK::Put(const std::string& key, const std::string& value, SDKError* error) {
    return true;
}

bool InsSDK::Get(const std::string& key, std::string* value, SDKError* error) {
    return true;
}
*/

} //namespace sdk
} //namespace ins
} //namespace galaxy

