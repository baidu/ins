#ifndef GALAXY_INS_SDK_H_
#define GALAXY_INS_SDK_H_

#include <stdint.h>
#include <vector>
#include <string>

namespace common {
    class Mutex;
    class ThreadPool;
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

struct KVPair {
    std::string key;
    std::string value;
};

class ScanResult;

typedef void (*WatchCallback)(const std::string& key,
                              const std::string& value,
                              const std::string& old_value,
                              bool has_key,
                              void* context);

class InsSDK {
public:
    static void ParseFlagFromArgs(int argc, char* argv[],
                                  std::vector<std::string>* members);
    InsSDK(const std::vector<std::string>& members);
    ~InsSDK();
    bool ShowCluster(std::vector<ClusterNodeInfo>* cluster_info);
    bool Put(const std::string& key, const std::string& value, SDKError* error);
    bool Get(const std::string& key, std::string* value, 
             SDKError* error);
    bool Delete(const std::string& key, SDKError* error);
    ScanResult* Scan(const std::string& start_key, 
                     const std::string& end_key);
    bool ScanOnce(const std::string& start_key,
                  const std::string& end_key,
                  std::vector<KVPair>* buffer,
                  SDKError* error);
    bool Watch(const std::string& key, 
               WatchCallback user_callback,
               void* context, 
               SDKError* error);

    static std::string StatusToString(int32_t status);

private:
    void PrepareServerList(std::vector<std::string>& server_list);
    void WatchTask(const std::string& key,
                   const std::string& old_value,
                   bool has_key,
                   WatchCallback user_callback,
                   void* context);
    std::string leader_id_;
    std::vector<std::string> members_;
    galaxy::RpcClient* rpc_client_;
    common::Mutex* mu_;
    common::ThreadPool* watch_pool_;
};


class ScanResult {
public:
    ScanResult(InsSDK* sdk);
    void Init(const std::string& start_key, const std::string& end_key);
    bool Done();
    SDKError Error();
    const std::string Key();
    const std::string Value();
    void Next();
private:
    std::vector<KVPair> buffer_;
    size_t offset_;
    InsSDK* sdk_;
    SDKError error_;
    std::string end_key_;
};

} //namespace sdk
} //namespace ins
} //namespace galaxy

#endif
