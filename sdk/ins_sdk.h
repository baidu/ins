#ifndef GALAXY_INS_SDK_H_
#define GALAXY_INS_SDK_H_

#include <vector>
#include <string>

namespace galaxy {
namespace ins {
namespace sdk {

enum NodeStatus{
    kLeader = 0,
    kCandidate = 1, 
    kFollower = 2,
    kOffline = 3
};

enum SDKError {
    kOK = 0,
    kClusterDown = 1,
    kNoSuchKey = 2,
    kTimeout = 3
};

struct ClusterInfo {
    std::string server_id;
    NodeStatus status;
    int64_t term;
};

class InsSDK {
public:
    InsSDK(const std::vector<std::string>& members);
    bool ShowCluster(std::vector<ClusterInfo>* cluster_info);
    bool Put(const std::string& key, const std::string& value, SDKError* error);
    bool Get(const std::string& key, std::string* value, SDKError* error);
private:
    std::string leader_id_;
    std::vector<std::string> members_;
};

} //namespace sdk
} //namespace ins
} //namespace galaxy

#endif
