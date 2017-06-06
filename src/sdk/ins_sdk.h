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
namespace ins {
    class RpcClient;
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
    kCleanBinlogFail = 5,
    kUserExists = 6,
    kPermissionDenied = 7,
    kPasswordError = 8,
    kUnknownUser = 9
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

struct StatInfo {
    int64_t current;
    int64_t average;
};

struct NodeStatInfo {
    std::string server_id;
    int32_t status;
    // Statistics is in proper order:
    //   Put, Get, Delete, Scan, KeepAlive, Lock, Unlock, Watch
    StatInfo stats[8];
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
    virtual ~InsSDK();

    virtual bool ShowCluster(std::vector<ClusterNodeInfo>* cluster_info);
    virtual bool Put(const std::string& key, const std::string& value, SDKError* error);
    virtual bool Get(const std::string& key, std::string* value,
                     SDKError* error);
    virtual bool Delete(const std::string& key, SDKError* error);
    virtual ScanResult* Scan(const std::string& start_key,
                             const std::string& end_key);
    virtual bool ScanOnce(const std::string& start_key,
                          const std::string& end_key,
                          std::vector<KVPair>* buffer,
                          SDKError* error);
    virtual bool Watch(const std::string& key,
                       WatchCallback user_callback,
                       void* context,
                       SDKError* error);
    virtual bool Lock(const std::string& key, SDKError* error); //may block
    virtual bool TryLock(const std::string& key, SDKError* error); //none block
    virtual bool UnLock(const std::string& key, SDKError* error);
    virtual bool Login(const std::string& username,
                       const std::string& password,
                       SDKError* error);
    virtual bool Logout(SDKError* error);
    virtual bool Register(const std::string& username,
                          const std::string& password,
                          SDKError* error);
    virtual bool CleanBinlog(const std::string& server_id,
                             int64_t end_index,
                             SDKError* error);
    virtual bool ShowStatistics(std::vector<NodeStatInfo>* statistics);
    virtual std::string GetSessionID();
    virtual std::string GetCurrentUserID();
    virtual bool IsLoggedIn();
    virtual void RegisterSessionTimeout(void (*handle_session_timeout)(void*), void* ctx );
    virtual void SetTimeoutTime(int64_t milliseconds);

    static std::string StatusToString(int32_t status);
    static std::string ErrorToString(SDKError error);

private:
    void Init(const std::vector<std::string>& members);
    void PrepareServerList(std::vector<std::string>& server_list);
    void KeepAliveTask();
    void KeepWatchTask(const std::string& key, 
                       const std::string& old_value,
                       bool key_exist,
                       std::string session_id,
                       int64_t watch_id);
    void MakeSessionID();
    void KeepWatchCallback(const galaxy::ins::WatchRequest* request,
                           galaxy::ins::WatchResponse* response,
                           bool failed, int error,
                           std::string server_id,
                           int64_t watch_id);
    void BackupWatchTask(const std::string& key, 
                         const std::string& old_value,
                         bool key_exist,
                         std::string session_id,
                         int64_t watch_id);
    static std::string HashPassword(const std::string& password);
    std::string leader_id_;
    std::string session_id_;
    std::string logged_uuid_;
    std::vector<std::string> members_;
    galaxy::ins::RpcClient* rpc_client_;
    ins_common::Mutex* mu_;
    ins_common::ThreadPool* keep_alive_pool_;
    bool is_keep_alive_bg_;
    bool stop_;
    std::set<std::string> watch_keys_;
    std::set<std::string> lock_keys_;
    std::map<std::string, WatchCallback> watch_cbs_;
    std::map<std::string, void*> watch_ctx_;
    ins_common::ThreadPool* keep_watch_pool_;
    void (*handle_session_timeout_) (void*);
    void * session_timeout_ctx_;
    int64_t last_succ_alive_timestamp_;
    int64_t watch_task_id_;
    std::set<int64_t> pending_watches_;
    bool loggin_expired_;
    int64_t timeout_time_;
};

class ScanResult {
public:
    ScanResult(InsSDK* sdk);
    virtual ~ScanResult() { }

    virtual void Init(const std::string& start_key,
                      const std::string& end_key);
    virtual bool Done();
    virtual SDKError Error();
    virtual const std::string Key();
    virtual const std::string Value();
    virtual void Next();
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
