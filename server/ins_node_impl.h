#ifndef GALAXY_INS_INS_NODE_IMPL_H_
#define GALAXY_INS_INS_NODE_IMPL_H_
#include "proto/ins_node.pb.h"

#include <stdint.h>
#include <string>
#include <vector>
#include <map>
#include "common/mutex.h"
#include "common/thread_pool.h"
#include "rpc/rpc_client.h"

namespace galaxy {
namespace ins {

class Meta;
class BinLogger;

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
    void BroadCastHeartBeat();
    void CheckLeaderCrash();
    void TryToBeLeader();
    int32_t GetRandomTimeout();
public:
    std::vector<std::string> members_;
private:
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
    std::map<std::string, std::string> data_map_;
    ThreadPool replicatter_;
    ThreadPool committer_;
};

void GetHostName(std::string* hostname);

} //namespace ins
} //namespace galaxy

#endif
