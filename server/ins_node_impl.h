#ifndef GALAXY_INS_INS_NODE_IMPL_H_
#define GALAXY_INS_INS_NODE_IMPL_H_
#include "proto/ins_node.pb.h"

#include <stdint.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <boost/shared_ptr.hpp>
#include "common/mutex.h"
#include "common/thread_pool.h"
#include "rpc/rpc_client.h"
#include "leveldb/db.h"

namespace galaxy {
namespace ins {

class Meta;
class BinLogger;

struct ClientAck {
    galaxy::ins::PutResponse* response;
    galaxy::ins::DelResponse* del_response;
    google::protobuf::Closure* done;
    ClientAck() : response(NULL),
                  del_response(NULL),
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
public:
    std::vector<std::string> members_;
private:
    bool stop_;
    std::string self_id_;
    int64_t current_term_;
    std::map<int64_t, std::string> voted_for_;
    std::map<int64_t, uint32_t> vote_grant_;
    std::vector<galaxy::ins::Entry> binlog_;
    galaxy::RpcClient rpc_client_;
    NodeStatus status_;
    Mutex mu_;
    ThreadPool pool_;
    ThreadPool heart_beat_pool_;
    int64_t elect_leader_task_;
    std::string current_leader_;
    int32_t heartbeat_count_;
    Meta * meta_;
    BinLogger* binlogger_;
    //for leaders
    leveldb::DB* data_store_;
    ThreadPool replicatter_;
    ThreadPool committer_;
    std::map<std::string, int64_t> next_index_;
    std::map<std::string, int64_t> match_index_;
    CondVar* replication_cond_;
    std::map<int64_t, ClientAck> client_ack_;
    std::set<std::string> replicating_;
    // for all servers
    int64_t commit_index_;
    int64_t last_applied_index_;
    CondVar* commit_cond_;
};

void GetHostName(std::string* hostname);

} //namespace ins
} //namespace galaxy

#endif
