#include "ins_sdk.h"

#include <algorithm>
#include <iterator>
#include <vector>
#include "proto/ins_node.pb.h"

namespace galaxy {
namespace ins {
namespace sdk {

InsSDK::InsSDK(const std::vector<std::string>& members) {
    std::copy(members.begin(), members.end(), std::back_inserter(members_));
}

bool InsSDK::ShowCluster(std::vector<ClusterInfo>* cluster_info) {
    return true;
}

bool InsSDK::Put(const std::string& key, const std::string& value, SDKError* error) {
    return true;
}

bool InsSDK::Get(const std::string& key, std::string* value, SDKError* error) {
    return true;
}


} //namespace sdk
} //namespace ins
} //namespace galaxy