#ifndef GALAXY_INS_SDK_H_
#define GALAXY_INS_SDK_H_

#include <stdint.h>
#include <set>
#include <map>
#include <vector>
#include <string>
#include <utility>

namespace ins_common {
    class Mutex;
    class ThreadPool;
}

namespace galaxy {
    class RpcClient;
}

namespace galaxy {
namespace ins {
    class WatchRequest;
    class WatchResponse;
}
}

namespace galaxy {
namespace ins {
namespace sdk {

enum SDKError {
    kOK = 0,
    kClusterDown = 1,
    kNoSuchKey = 2,
    kTimeout = 3,
    kLockFail = 4,
    kCleanBinlogFail = 5
};

struct ClusterNodeInfo {
    std::string server_id;
    int32_t status;
    int64_t term;
    int64_t last_log_index;
    int64_t last_log_term;
    int64_t commit_index;
    int64_t last_applied;
};

struct KVPair {
    std::string key;
    std::string value;
};

class ScanResult;

struct WatchParam {
    std::string key;
    std::string value;
    bool deleted;
    void* context;
};

typedef void (*WatchCallback)(const WatchParam& param, SDKError error);

class InsSDK {
public:
    static void ParseFlagFromArgs(int argc, char* argv[],
                                  std::vector<std::string>* members);
    InsSDK(const std::vector<std::string>& members);
    InsSDK(const std::string& server_list); //seperated by comma
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
    bool Lock(const std::string& key, SDKError* error); //may block
    bool TryLock(const std::string& key, SDKError *error); //none block
    bool UnLock(const std::string& key, SDKError* error);
    bool CleanBinlog(const std::string& server_id,
                     int64_t end_index, 
                     SDKError* error);
    std::string GetSessionID();
    void RegisterSessionTimeout(void (*handle_session_timeout)());
    static std::string StatusToString(int32_t status);

private:
    void Init(const std::vector<std::string>& members);
    void PrepareServerList(std::vector<std::string>& server_list);
    void KeepAliveTask();
    void KeepWatchTask(const std::string& key, 
                       const std::string& old_value,
                       bool key_exist);
    void MakeSessionID();
    void KeepWatchCallback(const galaxy::ins::WatchRequest* request,
                           galaxy::ins::WatchResponse* response,
                           bool failed, int error,
                           std::string server_id);
    std::string leader_id_;
    std::string session_id_;
    std::vector<std::string> members_;
    galaxy::RpcClient* rpc_client_;
    ins_common::Mutex* mu_;
    ins_common::ThreadPool* keep_alive_pool_;
    bool is_keep_alive_bg_;
    bool stop_;
    std::set<std::string> watch_keys_;
    std::set<std::string> lock_keys_;
    std::map<std::string, WatchCallback> watch_cbs_;
    std::map<std::string, void*> watch_ctx_;
    ins_common::ThreadPool* keep_watch_pool_;
    void (*handle_session_timeout_) ();
    int64_t last_succ_alive_timestamp_;
};

class ScanResult {
public:
    ScanResult(InsSDK* sdk);
    void Init(const std::string& start_key,
              const std::string& end_key);
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
