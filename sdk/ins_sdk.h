#ifndef GALAXY_INS_SDK_H_
#define GALAXY_INS_SDK_H_

#include <stdint.h>
#include <vector>
#include <string>

namespace common {
    class Mutex;
}

namespace galaxy {
    class RpcClient;
}

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
    int32_t status;
    int64_t term;
    int64_t last_log_index;
    int64_t last_log_term;
    int64_t commit_index;
};

class InsSDK {
public:
    static void ParseFlagFromArgs(int argc, char* argv[],
                                  std::vector<std::string>* members);
    InsSDK(const std::vector<std::string>& members);
    ~InsSDK();
    bool ShowCluster(std::vector<ClusterNodeInfo>* cluster_info);
    bool Put(const std::string& key, const std::string& value, SDKError* error);
    bool Get(const std::string& key, std::string* value, 
             SDKError* error, bool quorum = false);
    bool Delete(const std::string& key, SDKError* error);
    static std::string StatusToString(int32_t status);
private:
    void PrepareServerList(std::vector<std::string>& server_list);
    std::string leader_id_;
    std::vector<std::string> members_;
    galaxy::RpcClient* rpc_client_;
    common::Mutex* mu_;
};

} //namespace sdk
} //namespace ins
} //namespace galaxy

#endif
