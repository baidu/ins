#ifndef GALAXY_INS_INS_NODE_IMPL_H_
#define GALAXY_INS_INS_NODE_IMPL_H_
#include "proto/ins_node.pb.h"

#include <stdint.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <boost/shared_ptr.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/unordered_map.hpp>
#include "common/mutex.h"
#include "common/thread_pool.h"
#include "rpc/rpc_client.h"
#include "storage/storage_manage.h"
#include "server/user_manage.h"

using namespace boost::multi_index;

namespace galaxy {
namespace ins {

class Meta;
class BinLogger;

struct ClientAck {
    galaxy::ins::PutResponse* response;
    galaxy::ins::DelResponse* del_response;
    galaxy::ins::LockResponse* lock_response;
    galaxy::ins::UnLockResponse* unlock_response;
    galaxy::ins::LoginResponse* login_response;
    galaxy::ins::LogoutResponse* logout_response;
    galaxy::ins::RegisterResponse* register_response;
    google::protobuf::Closure* done;
    ClientAck() : response(NULL),
                  del_response(NULL),
                  lock_response(NULL),
                  unlock_response(NULL),
                  login_response(NULL),
                  logout_response(NULL),
                  register_response(NULL),
                  done(NULL) {
    }
};

struct ClientReadAck
{
    const galaxy::ins::GetRequest* request;
    galaxy::ins::GetResponse* response;
    google::protobuf::Closure* done;
    uint32_t succ_count ;
    uint32_t err_count;
    bool triggered;
    ClientReadAck() : request(NULL),
                      response(NULL),
                      done(NULL),
                      succ_count(0),
                      err_count(0),
                      triggered(false) {

    }
    typedef boost::shared_ptr<ClientReadAck> Ptr;
};

struct Session {
    std::string session_id;
    std::string uuid;
    int64_t last_report_time;
    Session() : last_report_time(0) {

    }
};

typedef multi_index_container<
    Session,
    indexed_by<
       hashed_unique<
         member<Session, std::string, &Session::session_id>
       >,
       ordered_non_unique<
         member<Session, int64_t, &Session::last_report_time>
       >
    >
> SessionContainer;

typedef SessionContainer::nth_index<0>::type SessionIDIndex;
typedef SessionContainer::nth_index<1>::type SessionTimeIndex;

struct WatchAck {
    WatchResponse* response;
    google::protobuf::Closure* done;
    WatchAck(WatchResponse* rsps,
             google::protobuf::Closure* dn) : response(rsps),
                                              done(dn) {

    }
    ~WatchAck() {
        if(done) {
            //fprintf(stderr, "~WatchAck\n");
            done->Run();
        }
    }
    typedef boost::shared_ptr<WatchAck> Ptr;
};

struct WatchEvent {
    std::string key;
    std::string session_id;
    WatchAck::Ptr ack;
};

typedef multi_index_container<
    WatchEvent,
    indexed_by<
        ordered_non_unique<
            member<WatchEvent, std::string, &WatchEvent::key> 
        >,
        ordered_non_unique<
            member<WatchEvent, std::string, &WatchEvent::session_id> 
        >
    >
> WatchEventContainer;

typedef WatchEventContainer::nth_index<0>::type WatchEventKeyIndex;
typedef WatchEventContainer::nth_index<1>::type WatchEventSessionIndex;

class InsNodeImpl : public InsNode {
public:
   
    InsNodeImpl(std::string& server_id, const std::vector<std::string>& members);
    virtual ~InsNodeImpl();
    void AppendEntries(::google::protobuf::RpcController* controller,
                       const ::galaxy::ins::AppendEntriesRequest* request,
                       ::galaxy::ins::AppendEntriesResponse* response,
                       ::google::protobuf::Closure* done);
    void Vote(::google::protobuf::RpcController* controller,
              const ::galaxy::ins::VoteRequest* request,
              ::galaxy::ins::VoteResponse* response,
              ::google::protobuf::Closure* done);
    void Put(::google::protobuf::RpcController* controller,
             const ::galaxy::ins::PutRequest* request,
             ::galaxy::ins::PutResponse* response,
             ::google::protobuf::Closure* done);
    void Get(::google::protobuf::RpcController* controller,
             const ::galaxy::ins::GetRequest* request,
             ::galaxy::ins::GetResponse* response,
             ::google::protobuf::Closure* done);
    void Delete(::google::protobuf::RpcController* controller,
                const ::galaxy::ins::DelRequest* request,
                ::galaxy::ins::DelResponse* response,
                ::google::protobuf::Closure* done);
    void ShowStatus(::google::protobuf::RpcController* controller,
                    const ::galaxy::ins::ShowStatusRequest* request,
                    ::galaxy::ins::ShowStatusResponse* response,
                    ::google::protobuf::Closure* done);
    void Scan(::google::protobuf::RpcController* controller,
              const ::galaxy::ins::ScanRequest* request,
              ::galaxy::ins::ScanResponse* response,
              ::google::protobuf::Closure* done);
    void KeepAlive(::google::protobuf::RpcController* controller,
                   const ::galaxy::ins::KeepAliveRequest* request,
                   ::galaxy::ins::KeepAliveResponse* response,
                   ::google::protobuf::Closure* done);
    void Lock(::google::protobuf::RpcController* controller,
              const ::galaxy::ins::LockRequest* request,
              ::galaxy::ins::LockResponse* response,
              ::google::protobuf::Closure* done);
    void UnLock(::google::protobuf::RpcController* controller,
                const ::galaxy::ins::UnLockRequest* request,
                ::galaxy::ins::UnLockResponse* response,
                ::google::protobuf::Closure* done);
    void Watch(::google::protobuf::RpcController* controller,
               const ::galaxy::ins::WatchRequest* request,
               ::galaxy::ins::WatchResponse* response,
               ::google::protobuf::Closure* done);
    void Login(::google::protobuf::RpcController* controller,
               const ::galaxy::ins::LoginRequest* request,
               ::galaxy::ins::LoginResponse* response,
               ::google::protobuf::Closure* done);
    void Logout(::google::protobuf::RpcController* controller,
                const ::galaxy::ins::LogoutRequest* request,
                ::galaxy::ins::LogoutResponse* response,
                ::google::protobuf::Closure* done);
    void Register(::google::protobuf::RpcController* controller,
                  const ::galaxy::ins::RegisterRequest* request,
                  ::galaxy::ins::RegisterResponse* response,
                  ::google::protobuf::Closure* done);
    void CleanBinlog(::google::protobuf::RpcController* controller,
                     const ::galaxy::ins::CleanBinlogRequest* request,
                     ::galaxy::ins::CleanBinlogResponse* response,
                     ::google::protobuf::Closure* done);
private:
    void VoteCallback(const ::galaxy::ins::VoteRequest* request,
                      ::galaxy::ins::VoteResponse* response,
                      bool failed, int error);
    void HearBeatCallback(const ::galaxy::ins::AppendEntriesRequest* request,
                          ::galaxy::ins::AppendEntriesResponse* response,
                          bool failed, int error);
    void HeartBeatForReadCallback(const ::galaxy::ins::AppendEntriesRequest* request,
                                 ::galaxy::ins::AppendEntriesResponse* response,
                                 bool failed, int error,
                                 ClientReadAck::Ptr context);
    void ForwardKeepAliveCallback(const ::galaxy::ins::KeepAliveRequest* request,
                                  ::galaxy::ins::KeepAliveResponse* response,
                                  bool failed, int error); 

    void BroadCastHeartBeat();
    void CheckLeaderCrash();
    void TryToBeLeader();
    int32_t GetRandomTimeout();
    void TransToFollower(const char* msg, int64_t new_term);
    void ReplicateLog(std::string follower_id);
    void StartReplicateLog();
    void GetLastLogIndexAndTerm(int64_t* last_log_index,
                                int64_t* last_log_term);
    void UpdateCommitIndex(int64_t a_index);
    void CommitIndexObserv();
    void TransToLeader();
    void RemoveExpiredSessions();
    void ParseValue(const std::string& value,
                    LogOperation& op, 
                    std::string& real_value);
    bool IsExpiredSession(const std::string& session_id);
    std::string BindKeyAndUser(const std::string& user, const std::string& key);
    std::string GetKeyFromEvent(const std::string& event_key);
    void RemoveEventBySession(const std::string& session_id);
    void TriggerEvent(const std::string& watch_key,
                      const std::string& key,
                      const std::string& value,
                      bool deleted);
    void TriggerEventWithParent(const std::string& key,
                                const std::string& value,
                                bool deleted);
    void TriggerEventBySessionAndKey(const std::string& session_id,
                                     const std::string& key,
                                     const std::string& value,
                                     bool deleted);
    void RemoveEventBySessionAndKey(const std::string& session_id,
                                    const std::string& key);
    void DelBinlog(int64_t index);
    bool LockIsAvailable(const std::string& user,
                         const std::string& key,
                         const std::string& session_id);
    void ForwardKeepAlive(const ::galaxy::ins::KeepAliveRequest * request,
                          ::galaxy::ins::KeepAliveResponse * response);
    void GarbageClean();
    void DoAppendEntries(const ::galaxy::ins::AppendEntriesRequest* request,
                         ::galaxy::ins::AppendEntriesResponse* response,
                         ::google::protobuf::Closure* done);
public:
    std::vector<std::string> members_;
private:
    bool stop_;
    std::string self_id_;
    int64_t current_term_;
    std::map<int64_t, std::string> voted_for_;
    std::map<int64_t, uint32_t> vote_grant_;
    std::vector<galaxy::ins::Entry> binlog_;
    galaxy::ins::RpcClient rpc_client_;
    NodeStatus status_;
    Mutex mu_;
    ThreadPool leader_crash_checker_;
    ThreadPool heart_beat_pool_;
    int64_t elect_leader_task_;
    std::string current_leader_;
    int32_t heartbeat_count_;
    Meta* meta_;
    BinLogger* binlogger_;
    UserManager* user_manager_;
    //for leaders
    StorageManager* data_store_;
    ThreadPool replicatter_;
    ThreadPool committer_;
    std::map<std::string, int64_t> next_index_;
    std::map<std::string, int64_t> match_index_;
    CondVar* replication_cond_;
    boost::unordered_map<int64_t, ClientAck> client_ack_;
    std::set<std::string> replicating_;
    int64_t heartbeat_read_timestamp_;
    bool in_safe_mode_;
    int64_t server_start_timestamp_;
    ThreadPool event_trigger_;
    // for all servers
    SessionContainer sessions_;
    Mutex sessions_mu_;
    ThreadPool session_checker_;
    int64_t commit_index_;
    int64_t last_applied_index_;
    CondVar* commit_cond_;
    WatchEventContainer watch_events_;
    Mutex watch_mu_;
    boost::unordered_map<std::string, std::set<std::string> > session_locks_;
    Mutex session_locks_mu_;
    ThreadPool binlog_cleaner_;
    ThreadPool follower_worker_;
    bool single_node_mode_;
    int64_t last_safe_clean_index_;
};

} //namespace ins
} //namespace galaxy

#endif
