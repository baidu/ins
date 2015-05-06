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
class InsNodeImpl : public InsNode {
public:
    enum InsNodeStatus {
        kLeader = 0,
        kCandidate = 1, 
        kFollower = 2
    };

    InsNodeImpl(std::string& server_id);
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
    void VoteCallback(const ::galaxy::ins::VoteRequest* request,
                      ::galaxy::ins::VoteResponse* response,
                      bool failed, int error);
    void CheckLeaderCrash();
    void TryToBeLeader();
public:
    std::vector<std::string> members_;
private:
    std::string self_id_;
    int64_t current_term_;
    std::map<int64_t, std::string> voted_for_;
    std::vector<galaxy::ins::Entry> binlog_;
    std::map<std::string, std::string> states_; 
    galaxy::RpcClient rpc_client_;
    InsNodeStatus status_;
    Mutex mu_;
    ThreadPool pool_;
    int64_t elect_leader_task_;
    std::string current_leader_;
};

void GetHostName(std::string* hostname);

} //namespace ins
} //namespace galaxy

