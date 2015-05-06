#include "ins_node_impl.h"
namespace galaxy {
namespace ins {

InsNodeImpl::InsNodeImpl () : current_term_(0) {

}

InsNodeImpl::~InsNodeImpl() {

}


void InsNodeImpl::AppendEntries(::google::protobuf::RpcController* controller,
                                const ::galaxy::ins::AppendEntriesRequest* request,
                                ::galaxy::ins::AppendEntriesResponse* response,
                                ::google::protobuf::Closure* done) {
    done->Run();
    return;
}

void InsNodeImpl::Vote(::google::protobuf::RpcController* controller,
                       const ::galaxy::ins::VoteRequest* request,
                       ::galaxy::ins::VoteResponse* response,
                       ::google::protobuf::Closure* done) {
    done->Run();
    return;
}

void InsNodeImpl::Put(::google::protobuf::RpcController* controller,
                      const ::galaxy::ins::PutRequest* request,
                      ::galaxy::ins::PutResponse* response,
                      ::google::protobuf::Closure* done) {
    done->Run();
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



