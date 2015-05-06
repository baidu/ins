#include "ins_node_impl.h"

#include <sys/utsname.h>
#include <boost/function.hpp>
#include <boost/bind.hpp>

namespace galaxy {
namespace ins {

void GetHostName(std::string* hostname) {
  struct utsname buf;
  if (0 != uname(&buf)) {
    *buf.nodename = '\0';
  }
  *hostname = buf.nodename;
}

InsNodeImpl::InsNodeImpl (std::string& server_id) : self_id_(server_id),
                                                    current_term_(0),
                                                    status_(kFollower) {
    srand(time(NULL));
    MutexLock lock(&mu_);
    CheckLeaderCrash();
}

InsNodeImpl::~InsNodeImpl() {

}

void InsNodeImpl::CheckLeaderCrash() {
    mu_.AssertHeld();
    int32_t timeout = 150 + (int32_t) (300.0 * rand()/(RAND_MAX+1.0));
    elect_leader_task_  = pool_.DelayTask(timeout, 
                                        boost::bind(&InsNodeImpl::TryToBeLeader,
                                                    this)
                                        );
}

void InsNodeImpl::VoteCallback(const ::galaxy::ins::VoteRequest* request,
                               ::galaxy::ins::VoteResponse* response,
                               bool failed, int error) {
    delete request;
    delete response;
}

void InsNodeImpl::TryToBeLeader() {
    LOG(INFO, "broad cast vote request to cluster");
    MutexLock lock(&mu_);
    current_term_++;
    status_ =  kCandidate;
    voted_for_[current_term_] = self_id_;
    std::vector<std::string>::iterator it = members_.begin();
    for(; it!= members_.end(); it++) {
        InsNode_Stub* stub;
        rpc_client_.GetStub(*it, &stub);
        ::galaxy::ins::VoteRequest* request = new ::galaxy::ins::VoteRequest();
        ::galaxy::ins::VoteResponse* response = new ::galaxy::ins::VoteResponse();
        request->set_candidate_id(self_id_);
        request->set_term(current_term_);
        /*rpc_client_.AsyncRequest(stub, &InsNode_Stub::Vote, request, response,
                                 boost::bind(&InsNodeImpl::VoteCallback, this),
                                 3, 0);*/
    }
}

void InsNodeImpl::AppendEntries(::google::protobuf::RpcController* controller,
                                const ::galaxy::ins::AppendEntriesRequest* request,
                                ::galaxy::ins::AppendEntriesResponse* response,
                                ::google::protobuf::Closure* done) {
    MutexLock lock(&mu_);
    if (request->term() >= current_term_) {
        status_ = kFollower;
    } else {
        response->set_current_term(current_term_);
        response->set_success(false);
        done->Run();
        return;
    }
    if (status_ == kFollower) {
        current_leader_ = request->leader_id();
        current_term_ = request->term();
        response->set_current_term(current_term_);
        response->set_success(true);
        done->Run();
    } else {
        LOG(FATAL, "invalid status: %d", status_);
        abort();
    }
    pool_.CancelTask(elect_leader_task_);
    CheckLeaderCrash();
    return;
}

void InsNodeImpl::Vote(::google::protobuf::RpcController* controller,
                       const ::galaxy::ins::VoteRequest* request,
                       ::galaxy::ins::VoteResponse* response,
                       ::google::protobuf::Closure* done) {
    MutexLock lock(&mu_);
    if (request->term() < current_term_) {
        response->set_vote_granted(false);
        response->set_term(current_term_);
        done->Run();
        return;
    }
    if (!voted_for_[request->term()].empty() &&
         voted_for_[request->term()] != request->candidate_id()) {
        response->set_vote_granted(false);
        response->set_term(current_term_);
        done->Run();
        return;
    }
    voted_for_[request->term()] = request->candidate_id();
    response->set_vote_granted(true);
    response->set_term(current_term_);
    done->Run();
    return;
}

void InsNodeImpl::Put(::google::protobuf::RpcController* controller,
                      const ::galaxy::ins::PutRequest* request,
                      ::galaxy::ins::PutResponse* response,
                      ::google::protobuf::Closure* done) {
    return;
}

void InsNodeImpl::Get(::google::protobuf::RpcController* controller,
                      const ::galaxy::ins::GetRequest* request,
                      ::galaxy::ins::GetResponse* response,
                      ::google::protobuf::Closure* done) {
    done->Run();
    return;
}


} //namespace ins
} //namespace galaxy



