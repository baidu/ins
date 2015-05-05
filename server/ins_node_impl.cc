#include "ins_node_impl.h"
namespace galaxy {
namespace ins {

InsNodeImpl::InsNodeImpl () {

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

} //namespace ins
} //namespace galaxy



