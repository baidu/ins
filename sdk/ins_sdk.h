#ifndef GALAXY_INS_SDK_H_
#define GALAXY_INS_SDK_H_

#include <vector>
#include <string>
#include "common/event.h"
#include "common/mutex.h"
#include "rpc/rpc_client.h"
#include "proto/ins_node.pb.h"

namespace galaxy {
namespace ins {
namespace sdk {

enum SDKError {
    kOK = 0,
    kClusterDown = 1,
    kNoSuchKey = 2,
    kTimeout = 3
};

struct ClusterNodeInfo {
    std::string server_id;
    galaxy::ins::NodeStatus status;
    int64_t term;
    int64_t last_log_index;
    int64_t last_log_term;
};

class InsSDK {
public:
    InsSDK(const std::vector<std::string>& members);
    bool ShowCluster(std::vector<ClusterNodeInfo>* cluster_info);
    bool Put(const std::string& key, const std::string& value, SDKError* error);
    bool Get(const std::string& key, std::string* value, SDKError* error);
    static std::string StatusToString(galaxy::ins::NodeStatus status);
private:
    std::string leader_id_;
    std::vector<std::string> members_;
    galaxy::RpcClient rpc_client_;
};

} //namespace sdk
} //namespace ins
} //namespace galaxy

#endif
