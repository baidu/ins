#include "ins_node_impl.h"

#include <assert.h>
#include <sys/utsname.h>
#include <boost/algorithm/string/replace.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/scoped_ptr.hpp>
#include <gflags/gflags.h>
#include "common/this_thread.h"
#include "common/timer.h"
#include "storage/meta.h"
#include "storage/binlog.h"

DECLARE_string(ins_data_dir);
DECLARE_string(ins_binlog_dir);
DECLARE_int32(max_cluster_size);
DECLARE_int32(log_rep_batch_max);
DECLARE_int32(replication_retry_timespan);
DECLARE_int32(elect_timeout_min);
DECLARE_int32(elect_timeout_max);
DECLARE_int64(session_expire_timeout);

const std::string tag_last_applied_index = "#TAG_LAST_APPLIED_INDEX#";

namespace galaxy {
namespace ins {

InsNodeImpl::InsNodeImpl(std::string& server_id,
                         const std::vector<std::string>& members
                         ) : stop_(false),
                             self_id_(server_id),
                             current_term_(0),
                             status_(kFollower),
                             heartbeat_count_(0),
                             meta_(NULL),
                             binlogger_(NULL),
                             user_manager_(NULL),
                             replicatter_(FLAGS_max_cluster_size),
                             heartbeat_read_timestamp_(0),
                             in_safe_mode_(true),
                             server_start_timestamp_(0),
                             commit_index_(-1),
                             last_applied_index_(-1),
                             single_node_mode_(false){
    srand(time(NULL));
    replication_cond_ = new CondVar(&mu_);
    commit_cond_ = new CondVar(&mu_);
    std::vector<std::string>::const_iterator it = members.begin();
    bool self_in_cluster = false;
    for(; it != members.end(); it++) {
        members_.push_back(*it);
        if (self_id_ == *it) {
            LOG(INFO, "cluster member[Self]: %s", it->c_str());
            self_in_cluster = true;
        } else {
            LOG(INFO, "cluster member: %s", it->c_str());
        }
    }
    if (!self_in_cluster) {
        LOG(FATAL, "this node is not in cluster membership,"
                   " please check your configuration. self: %s", self_id_.c_str());
        exit(-1);
    }
    if (members_.size() > static_cast<size_t>(FLAGS_max_cluster_size)) {
        LOG(FATAL, "cluster size is larger than configuration: %d > %d",
            members_.size(), FLAGS_max_cluster_size);
        exit(-1);
    }
    if (members_.size() == 1) {
        single_node_mode_ = true;
    }
    std::string sub_dir = self_id_;
    boost::replace_all(sub_dir, ":", "_");
    
    meta_ = new Meta(FLAGS_ins_data_dir + "/" + sub_dir);
    binlogger_ = new BinLogger(FLAGS_ins_binlog_dir + "/" + sub_dir);
    user_manager_ = new UserManager();
    meta_->ReadUserList(user_manager_);
    current_term_ = meta_->ReadCurrentTerm();
    meta_->ReadVotedFor(voted_for_);
    
    std::string data_store_path = FLAGS_ins_data_dir + "/" 
                                  + sub_dir + "/store" ;
    data_store_ = new StorageManager(data_store_path);
    std::string tag_value;
    Status status = data_store_->Get(StorageManager::anonymous_user,
                                     tag_last_applied_index,
                                     &tag_value);
    if (status == kOk) {
        last_applied_index_ =  BinLogger::StringToInt(tag_value);
    }
    server_start_timestamp_ = ins_common::timer::get_micros();
    committer_.AddTask(boost::bind(&InsNodeImpl::CommitIndexObserv, this));
    MutexLock lock(&mu_);
    CheckLeaderCrash();
    session_checker_.AddTask( 
        boost::bind(&InsNodeImpl::RemoveExpiredSessions, this)
    );
}

InsNodeImpl::~InsNodeImpl() {
    {
        MutexLock lock(&mu_);
        stop_ = true;
        commit_cond_->Signal();
        replication_cond_->Broadcast();
    }
    replicatter_.Stop(true);
    committer_.Stop(true);
    leader_crash_checker_.Stop(true);
    heart_beat_pool_.Stop(true);
    session_checker_.Stop(true);
    event_trigger_.Stop(true);
    binlog_cleaner_.Stop(true);
    {
        MutexLock lock(&mu_);
        delete meta_;
        delete binlogger_;
    }
}

int32_t InsNodeImpl::GetRandomTimeout() {
    float span = FLAGS_elect_timeout_max - FLAGS_elect_timeout_min;
    int32_t timeout = FLAGS_elect_timeout_min + 
                      (int32_t) (span * rand()/(RAND_MAX+1.0));
    //LOG(INFO, "random timeout %ld", timeout);
    return timeout;
}

void InsNodeImpl::CheckLeaderCrash() {
    mu_.AssertHeld();
    if (stop_) {
        return;
    }
    int32_t timeout = GetRandomTimeout();
    elect_leader_task_  = leader_crash_checker_.DelayTask(timeout, 
                                        boost::bind(&InsNodeImpl::TryToBeLeader,
                                                    this)
                                        );
    //LOG(INFO, "InsNodeImpl::CheckLeaderCrash, elect_leader_task_:%d", elect_leader_task_);
}

void InsNodeImpl::ShowStatus(::google::protobuf::RpcController* /*controller*/,
                             const ::galaxy::ins::ShowStatusRequest* /*request*/,
                             ::galaxy::ins::ShowStatusResponse* response,
                             ::google::protobuf::Closure* done) {
    MutexLock lock(&mu_);
    int64_t last_log_index;
    int64_t last_log_term;
    GetLastLogIndexAndTerm(&last_log_index, &last_log_term);
    response->set_status(status_);
    response->set_term(current_term_);    
    response->set_last_log_index(last_log_index);
    response->set_last_log_term(last_log_term);
    response->set_commit_index(commit_index_);
    response->set_last_applied(last_applied_index_);
    done->Run();
}

void InsNodeImpl::TransToFollower(const char* msg, int64_t new_term) {
    mu_.AssertHeld();
    LOG(INFO, "%s, my term is outdated(%ld < %ld), trans to follower",
        msg, 
        current_term_, 
        new_term);
    status_ = kFollower;
    current_term_ = new_term;
    meta_->WriteCurrentTerm(current_term_);
}

void InsNodeImpl::CommitIndexObserv() {
    MutexLock lock(&mu_);
    while (!stop_) {
        while (!stop_ && commit_index_ <=  last_applied_index_) {
            LOG(DEBUG, "commit_idx: %ld, last_applied_index: %ld",
                commit_index_, last_applied_index_);
            commit_cond_->Wait();
        }
        if (stop_) {
            return;
        }
        int64_t from_idx = last_applied_index_;
        int64_t to_idx = commit_index_;
        bool nop_committed = false;
        mu_.Unlock();
        for (int64_t i = from_idx + 1; i <= to_idx; i++) {
            LogEntry log_entry;
            bool slot_ok = binlogger_->ReadSlot(i, &log_entry);
            assert(slot_ok);
            Status s;
            std::string type_and_value;
            bool lock_succ = true;
            switch(log_entry.op) {
                case kPut:
                case kLock:
                    LOG(DEBUG, "add to data_store_, key: %s, value: %s",
                        log_entry.key.c_str(), log_entry.value.c_str());
                    type_and_value.append(1, static_cast<char>(log_entry.op));
                    type_and_value.append(log_entry.value);
                    s = data_store_->Put(log_entry.user, log_entry.key, type_and_value);
                    event_trigger_.AddTask(
                        boost::bind(&InsNodeImpl::TriggerEventWithParent,
                                    this,
                                    log_entry.key, log_entry.value, false)
                    );
                    if (log_entry.op == kLock) {
                        MutexLock lock_sk(&session_locks_mu_);
                        session_locks_[log_entry.value].insert(log_entry.key);
                    }
                    assert(s == kOk);
                    break;
                case kDel:
                    LOG(INFO, "delete from data_store_, key: %s",
                        log_entry.key.c_str());
                    s = data_store_->Delete(log_entry.user, log_entry.key);
                    assert(s == kOk);
                    event_trigger_.AddTask(
                        boost::bind(&InsNodeImpl::TriggerEventWithParent,
                                    this,
                                    log_entry.key, log_entry.value, true)
                    );
                    break;
                case kNop:
                    LOG(DEBUG, "kNop got, do nothing, key: %s", 
                              log_entry.key.c_str());
                    nop_committed = true;
                    break;
                case kUnLock:
                    {   
                        std::string key = log_entry.key;
                        std::string old_session = log_entry.value;
                        std::string value;
                        s = data_store_->Get(log_entry.user, key, &value);
                        if (s == kOk) {
                            std::string cur_session;
                            LogOperation op;
                            ParseValue(value, op, cur_session);
                            if (op == kLock && cur_session == old_session) { //DeleteIf
                                s = data_store_->Delete(log_entry.user, key);
                                assert(s == kOk);
                                LOG(INFO, "unlock on %s", key.c_str());
                                event_trigger_.AddTask(
                                  boost::bind(&InsNodeImpl::TriggerEventWithParent,
                                              this,
                                              key, old_session, true)
                                );
                            }
                        }
                    }
                    break;
            }
            mu_.Lock();
            if (status_ == kLeader && nop_committed) {
                in_safe_mode_ = false;
                LOG(INFO, "Leave safe mode now");
            }
            if (status_ == kLeader && client_ack_.find(i) != client_ack_.end()) {
                ClientAck& ack = client_ack_[i];
                if (ack.response) {
                    ack.response->set_success(true);
                    ack.response->set_leader_id("");
                    ack.done->Run(); //client put ok;
                }
                if (ack.del_response) {
                    ack.del_response->set_success(true);
                    ack.del_response->set_leader_id("");
                    ack.done->Run(); //client del ok;   
                }
                if (ack.lock_response) {
                    if (lock_succ) {
                        ack.lock_response->set_success(true);
                        ack.lock_response->set_leader_id("");
                    } else {
                        ack.lock_response->set_success(false);
                        ack.lock_response->set_leader_id("");
                    }
                    ack.done->Run(); //client lock ok;   
                }
                if (ack.unlock_response) {
                    ack.unlock_response->set_success(true);
                    ack.unlock_response->set_leader_id("");
                    ack.done->Run(); //client unlock ok;
                }
                client_ack_.erase(i);
            }
            last_applied_index_ += 1;
            Status sp = data_store_->Put(StorageManager::anonymous_user,
                                         tag_last_applied_index, 
                                         BinLogger::IntToString(last_applied_index_));
            assert(sp == kOk);
            mu_.Unlock();
        }
        mu_.Lock();
    }
}

void InsNodeImpl::ForwardKeepAliveCallback(
                                  const ::galaxy::ins::KeepAliveRequest* request,
                                  ::galaxy::ins::KeepAliveResponse* response,
                                  bool /*failed*/, int /*error*/) {
    boost::scoped_ptr<const galaxy::ins::KeepAliveRequest> request_ptr(request);
    boost::scoped_ptr<galaxy::ins::KeepAliveResponse> response_ptr(response);
    LOG(DEBUG, "heartbeat from clients forwarded");
}

void InsNodeImpl::HearBeatCallback(const ::galaxy::ins::AppendEntriesRequest* request,
                                  ::galaxy::ins::AppendEntriesResponse* response,
                                  bool failed, int /*error*/) {
    MutexLock lock(&mu_);
    boost::scoped_ptr<const galaxy::ins::AppendEntriesRequest> request_ptr(request);
    boost::scoped_ptr<galaxy::ins::AppendEntriesResponse> response_ptr(response);
    if (status_ != kLeader) {
        LOG(INFO, "outdated HearBeatCallback, I am no longer leader now.");
        return ;
    }
    if (!failed) {
        if (response_ptr->current_term() > current_term_) {
            TransToFollower("InsNodeImpl::HearBeatCallback", 
                            response_ptr->current_term());
        }
        else {
            //LOG(INFO, "I am the leader at term: %ld", current_term_);
        }
    }  
}

void InsNodeImpl::HeartBeatForReadCallback(
                              const ::galaxy::ins::AppendEntriesRequest* request,
                              ::galaxy::ins::AppendEntriesResponse* response,
                              bool failed, int /*error*/,
                              ClientReadAck::Ptr context) {
    MutexLock lock(&mu_);
    boost::scoped_ptr<const galaxy::ins::AppendEntriesRequest> request_ptr(request);
    boost::scoped_ptr<galaxy::ins::AppendEntriesResponse> response_ptr(response);
    if (context->triggered) {
        return;
    }
    if (status_ != kLeader) {
        LOG(INFO, "outdated HearBeatCallbackForRead, I am no longer leader now.");
        context->response->set_success(false);
        context->response->set_hit(false);
        context->response->set_leader_id("");
        context->done->Run();
        context->triggered = true;
        return ;
    }
    if (!failed) {
        if (response_ptr->current_term() > current_term_) {
            TransToFollower("InsNodeImpl::HeartBeatCallbackForRead", 
                            response_ptr->current_term());
            context->response->set_success(false);
            context->response->set_hit(false);
            context->response->set_leader_id("");
            context->done->Run();
            context->triggered = true;
            return ;
        }
        else {
            context->succ_count += 1;
        }
    } else {
        context->err_count += 1;
    }
    if (context->succ_count > members_.size() / 2) {
        std::string key = context->request->key();
        LOG(DEBUG, "client get key: %s", key.c_str());
        Status s;
        std::string value;
        s = data_store_->Get(StorageManager::anonymous_user, key, &value);
        std::string real_value;
        LogOperation op;
        ParseValue(value, op, real_value);
        if (s == kOk) {
            if (op == kLock) {
                if (IsExpiredSession(real_value)) {
                    context->response->set_hit(false);
                    context->response->set_success(true);
                    context->response->set_leader_id("");
                } else{
                    context->response->set_hit(true);
                    context->response->set_success(true);
                    context->response->set_value(real_value);
                    context->response->set_leader_id("");
                }
            } else{
                context->response->set_hit(true);
                context->response->set_success(true);
                //LOG(INFO, "get value: %s", real_value.c_str());
                context->response->set_value(real_value);
                context->response->set_leader_id("");
            }
        } else {
            context->response->set_hit(false);
            context->response->set_success(true);
            context->response->set_leader_id("");
        }
        context->done->Run();
        context->triggered = true;
        heartbeat_read_timestamp_ = ins_common::timer::get_micros();
    }
    if (context->err_count > members_.size() / 2) {
        context->response->set_success(false);
        context->response->set_hit(false);
        context->response->set_leader_id("");
        context->done->Run();
        context->triggered = true;
    }
}

void InsNodeImpl::BroadCastHeartBeat() {
    MutexLock lock(&mu_);
    if (stop_) {
        return;
    }
    if (status_ != kLeader) {
        return;
    }
    //LOG(INFO,"broadcast heartbeat to clusters");
    std::vector<std::string>::iterator it = members_.begin();
    for(; it!= members_.end(); it++) {
        if (*it == self_id_) {
            continue;
        }
        InsNode_Stub* stub;
        rpc_client_.GetStub(*it, &stub);
        boost::scoped_ptr<galaxy::ins::InsNode_Stub> stub_guard(stub);
        ::galaxy::ins::AppendEntriesRequest* request = 
                    new ::galaxy::ins::AppendEntriesRequest();
        ::galaxy::ins::AppendEntriesResponse* response =
                    new ::galaxy::ins::AppendEntriesResponse();
        request->set_term(current_term_);
        request->set_leader_id(self_id_);
        request->set_leader_commit_index(commit_index_);
        boost::function<void (const ::galaxy::ins::AppendEntriesRequest*,
                        ::galaxy::ins::AppendEntriesResponse*,
                        bool, int) > callback;
        callback = boost::bind(&InsNodeImpl::HearBeatCallback, this,
                               _1, _2, _3, _4);
        rpc_client_.AsyncRequest(stub, &InsNode_Stub::AppendEntries, 
                                 request, response, callback, 2, 1);
    }
    heart_beat_pool_.DelayTask(50, boost::bind(&InsNodeImpl::BroadCastHeartBeat, this));
}

void InsNodeImpl::StartReplicateLog() {
    mu_.AssertHeld();
    LOG(INFO, "StartReplicateLog");
    std::vector<std::string>::iterator it = members_.begin();
    for(; it!= members_.end(); it++) {
        if (*it == self_id_) {
            continue;
        }
        if (replicating_.find(*it) != replicating_.end()){
            LOG(INFO, "there is another thread replicating on : %s",
                it->c_str());
            continue;
        }
        std::string follower_id = *it;
        next_index_[follower_id] = binlogger_->GetLength();
        match_index_[follower_id] = -1;
        replicatter_.AddTask(boost::bind(&InsNodeImpl::ReplicateLog,
                                         this, *it));
    }
    LogEntry log_entry;
    log_entry.key = "Ping";
    log_entry.value = "";
    log_entry.term = current_term_;
    log_entry.op = kNop;
    binlogger_->AppendEntry(log_entry);
}

void InsNodeImpl::TransToLeader() {
    mu_.AssertHeld();
    in_safe_mode_ = true;
    status_ = kLeader;
    current_leader_ = self_id_;
    LOG(INFO, "I win the election, term:%d", current_term_);
    heart_beat_pool_.AddTask(
        boost::bind(&InsNodeImpl::BroadCastHeartBeat, this));
    StartReplicateLog();
}

void InsNodeImpl::VoteCallback(const ::galaxy::ins::VoteRequest* request,
                               ::galaxy::ins::VoteResponse* response,
                               bool failed, int /*error*/) {
    MutexLock lock(&mu_);
    boost::scoped_ptr<const galaxy::ins::VoteRequest> request_ptr(request);
    boost::scoped_ptr<galaxy::ins::VoteResponse> response_ptr(response);
    if (!failed && status_ == kCandidate) {
        int64_t their_term = response_ptr->term();
        LOG(INFO, "InsNodeImpl::VoteCallback[%ld], result:%s",
            their_term, response_ptr->vote_granted()?"true":"false");
        if (response_ptr->vote_granted() && their_term == current_term_) {
            vote_grant_[current_term_]++;
            if (vote_grant_[current_term_] > (members_.size() / 2)) {
                TransToLeader();
            }
        } else {
            if (their_term > current_term_) {
                TransToFollower("InsNodeImpl::VoteCallback", their_term);
            }
        }
    }
}

void InsNodeImpl::GetLastLogIndexAndTerm(int64_t* last_log_index,
                                         int64_t* last_log_term) {
    mu_.AssertHeld();
    *last_log_index = binlogger_->GetLength() - 1;
    *last_log_term = -1;
    if (*last_log_index >= 0) {
        LogEntry log_entry;
        bool slot_ok = binlogger_->ReadSlot(*last_log_index, &log_entry);
        assert(slot_ok);
        *last_log_term = log_entry.term;
    }
}

void InsNodeImpl::TryToBeLeader() {
    MutexLock lock(&mu_);
    if (single_node_mode_) { //single node mode
        status_ = kLeader;
        current_leader_ =  self_id_;
        in_safe_mode_ = false;
        commit_index_ = last_applied_index_;
        current_term_++;
        meta_->WriteCurrentTerm(current_term_);
        return;
    }
    if (status_ == kLeader) {
        CheckLeaderCrash();
        return;
    }
    if (status_ == kFollower && heartbeat_count_ > 0) {
        heartbeat_count_ = 0;
        CheckLeaderCrash();
        return;
    }
    current_term_++;
    meta_->WriteCurrentTerm(current_term_);
    status_ =  kCandidate;
    voted_for_[current_term_] = self_id_;
    meta_->WriteVotedFor(current_term_, self_id_);
    vote_grant_[current_term_] ++;
    std::vector<std::string>::iterator it = members_.begin();
    int64_t last_log_index;
    int64_t last_log_term;
    GetLastLogIndexAndTerm(&last_log_index, &last_log_term);
    LOG(INFO, "broad cast vote request to cluster, new term: %ld", current_term_);
    for(; it!= members_.end(); it++) {
        if (*it == self_id_) {
            continue;
        }
        InsNode_Stub* stub;
        rpc_client_.GetStub(*it, &stub);
        boost::scoped_ptr<galaxy::ins::InsNode_Stub> stub_guard(stub);
        ::galaxy::ins::VoteRequest* request = new ::galaxy::ins::VoteRequest();
        ::galaxy::ins::VoteResponse* response = new ::galaxy::ins::VoteResponse();
        request->set_candidate_id(self_id_);
        request->set_term(current_term_);
        request->set_last_log_index(last_log_index);
        request->set_last_log_term(last_log_term);
        boost::function<void (const ::galaxy::ins::VoteRequest* ,
                              ::galaxy::ins::VoteResponse* ,
                              bool, int ) > callback;
        callback = boost::bind(&InsNodeImpl::VoteCallback, this, _1, _2, _3, _4);
        rpc_client_.AsyncRequest(stub, &InsNode_Stub::Vote, request, response,
                                 callback, 2, 1);
    }
    CheckLeaderCrash();
}

void InsNodeImpl::AppendEntries(::google::protobuf::RpcController* /*controller*/,
                                const ::galaxy::ins::AppendEntriesRequest* request,
                                ::galaxy::ins::AppendEntriesResponse* response,
                                ::google::protobuf::Closure* done) {
    MutexLock lock(&mu_);
    if (request->term() >= current_term_) {
        status_ = kFollower;
        if (request->term() > current_term_) {
            meta_->WriteCurrentTerm(request->term());
        }
        current_term_ = request->term();
    } else {
        response->set_current_term(current_term_);
        response->set_success(false);
        response->set_log_length(binlogger_->GetLength());
        LOG(INFO, "[AppendEntries] term is outdated");
        done->Run();
        return;
    }

    if (status_ == kFollower) {
        current_leader_ = request->leader_id();
        heartbeat_count_++;
        if (request->entries_size() > 0) {
            if (request->prev_log_index() >= binlogger_->GetLength()){
                response->set_current_term(current_term_);
                response->set_success(false);
                response->set_log_length(binlogger_->GetLength());
                LOG(INFO, "[AppendEntries] prev log is beyond");
                done->Run();
                return;
            }
            int64_t prev_log_term = -1;
            if (request->prev_log_index() >= 0) {
                LogEntry prev_log_entry;
                bool slot_ok = binlogger_->ReadSlot(request->prev_log_index(),
                                                    &prev_log_entry);
                assert(slot_ok);
                prev_log_term = prev_log_entry.term;
            }
            if (prev_log_term != request->prev_log_term() ) {
                binlogger_->Truncate(request->prev_log_index() - 1);
                response->set_current_term(current_term_);
                response->set_success(false);
                response->set_log_length(binlogger_->GetLength());
                LOG(INFO, "[AppendEntries] term not match, "
                    "term: %ld,%ld", 
                    prev_log_term, request->prev_log_term());
                done->Run();
                return;
            }
            if (binlogger_->GetLength() > request->prev_log_index() + 1) {
                int64_t old_length = binlogger_->GetLength();
                binlogger_->Truncate(request->prev_log_index() );
                LOG(INFO, "[AppendEntries] log length alignment, "
                    "length: %ld,%ld", 
                    old_length, request->prev_log_index());
            }
            binlogger_->AppendEntryList(request->entries());
        }
        int64_t old_commit_index = commit_index_;
        commit_index_ = std::min(binlogger_->GetLength() - 1,
                                 request->leader_commit_index());
       
        if (commit_index_ > old_commit_index) {
            commit_cond_->Signal();
            LOG(DEBUG, "follower: update my commit index to :%ld", commit_index_);
        }
        response->set_current_term(current_term_);
        response->set_success(true);
        response->set_log_length(binlogger_->GetLength());
        done->Run();
    } else {
        LOG(FATAL, "invalid status: %d", status_);
        abort();
    }
    return;
}

void InsNodeImpl::Vote(::google::protobuf::RpcController* /*controller*/,
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
    int64_t last_log_index;
    int64_t last_log_term;
    GetLastLogIndexAndTerm(&last_log_index, &last_log_term);
    if (request->last_log_term() < last_log_term) {
        response->set_vote_granted(false);
        response->set_term(current_term_);
        done->Run();
        return;
    } else if(request->last_log_term() == last_log_term) {
        if (request->last_log_index() < last_log_index) {
            response->set_vote_granted(false);
            response->set_term(current_term_);
            done->Run();
            return;
        }
    }

    if (request->term() > current_term_) {
        TransToFollower("InsNodeImpl::Vote", request->term());
    }
    if (voted_for_.find(current_term_) != voted_for_.end() &&
        voted_for_[current_term_] != request->candidate_id()) {
        response->set_vote_granted(false);
        response->set_term(current_term_);
        done->Run();
        return;
    }
    voted_for_[current_term_] = request->candidate_id();
    meta_->WriteVotedFor(current_term_, request->candidate_id());
    response->set_vote_granted(true);
    response->set_term(current_term_);
    done->Run();
    return;
}


void InsNodeImpl::UpdateCommitIndex(int64_t a_index) {
    mu_.AssertHeld();
    std::vector<std::string>::const_iterator it;
    uint32_t match_count = 0;
    for (it = members_.begin(); it != members_.end(); it++) {
        std::string server_id = *it;
        if (match_index_[server_id] >= a_index) {
            match_count += 1;
        }
    }
    if (match_count >= match_index_.size()/2 && a_index > commit_index_) {
        commit_index_ = a_index;
        LOG(DEBUG, "update to new commit index: %ld", commit_index_);
        commit_cond_->Signal();
    }
}

void InsNodeImpl::ReplicateLog(std::string follower_id) {
    MutexLock lock(&mu_);
    replicating_.insert(follower_id);
    while (!stop_ && status_ == kLeader) {
        while (!stop_ && binlogger_->GetLength() <= next_index_[follower_id]) {
            LOG(DEBUG, "no new log entry for %s", follower_id.c_str());
            replication_cond_->TimeWait(2000);
            if (status_ != kLeader) {
                break;
            }
        }
        if (stop_) {
            break;
        }
        if (status_ != kLeader) {
            LOG(INFO, "stop realicate log, no longger leader"); 
            break;
        }
        int64_t index = next_index_[follower_id];
        int64_t cur_term = current_term_;
        int64_t prev_index = index - 1;
        int64_t prev_term = -1;
        int64_t cur_commit_index = commit_index_;
        int64_t batch_span = binlogger_->GetLength() - index;
        batch_span = std::min(batch_span, 
                              static_cast<int64_t>(FLAGS_log_rep_batch_max));
        std::string leader_id = self_id_;
        LogEntry prev_log_entry;
        if (prev_index > -1) {
            bool slot_ok = binlogger_->ReadSlot(prev_index, &prev_log_entry);
            if (!slot_ok) {
                LOG(FATAL, "bad slot [%ld], can't replicate on %s ", 
                    prev_index, follower_id.c_str());
                break;
            }
            prev_term = prev_log_entry.term;
        } 
        mu_.Unlock();

        InsNode_Stub* stub;
        int64_t max_term = -1;
        rpc_client_.GetStub(follower_id, &stub);
        boost::scoped_ptr<galaxy::ins::InsNode_Stub> stub_guard(stub);
        galaxy::ins::AppendEntriesRequest request;
        galaxy::ins::AppendEntriesResponse response;
        request.set_term(cur_term);
        request.set_leader_id(leader_id);
        request.set_prev_log_index(prev_index);
        request.set_prev_log_term(prev_term);
        request.set_leader_commit_index(cur_commit_index);
        bool has_bad_slot = false;
        for (int idx = index; idx < (index + batch_span); idx++) {
            LogEntry log_entry;
            bool slot_ok = binlogger_->ReadSlot(idx, &log_entry);
            if (!slot_ok) {
                has_bad_slot = true;
            }
            galaxy::ins::Entry * entry = request.add_entries();
            entry->set_term(log_entry.term);
            entry->set_key(log_entry.key);
            entry->set_value(log_entry.value);
            entry->set_op(log_entry.op);
            max_term = std::max(max_term, log_entry.term);
        }
        if (has_bad_slot) {
            LOG(FATAL, "bad slot, can't replicate on server: %s", follower_id.c_str());
            mu_.Lock();
            break;
        }
        bool ok = rpc_client_.SendRequest(stub, 
                                          &InsNode_Stub::AppendEntries,
                                          &request,
                                          &response,
                                          5, 1);
        mu_.Lock();
        mu_.AssertHeld();
        if (ok && response.current_term() > current_term_) {
            TransToFollower("InsNodeImpl::ReplicateLog", 
                            response.current_term());
        }
        if (status_ != kLeader) {
            LOG(INFO, "stop realicate log, no longger leader"); 
            break;
        }
        if (ok) {
            if (response.success()) { // log replicated
                next_index_[follower_id] = index + batch_span;
                match_index_[follower_id] = index + batch_span - 1;
                if (max_term == current_term_) {
                    UpdateCommitIndex(index + batch_span - 1);
                }
            } else { // (index, term ) miss match
                next_index_[follower_id] -= 1;
                next_index_[follower_id] = std::min(next_index_[follower_id],
                                                    response.log_length());
                LOG(INFO, "adjust next_index of %s to %ld",
                    follower_id.c_str(), 
                    next_index_[follower_id]);
                if (next_index_[follower_id] < 0 ){
                    next_index_[follower_id] = 0;
                }
            }
        } else { //rpc error;
            mu_.Unlock();
            LOG(FATAL, "faild to send replicate-rpc to %s ", 
                follower_id.c_str());
            ThisThread::Sleep(FLAGS_replication_retry_timespan);
            mu_.Lock();
        }
    }
    replicating_.erase(follower_id);
}

void InsNodeImpl::Get(::google::protobuf::RpcController* /*controller*/,
                      const ::galaxy::ins::GetRequest* request,
                      ::galaxy::ins::GetResponse* response,
                      ::google::protobuf::Closure* done) {
    MutexLock lock(&mu_);
    if (status_ == kFollower) {
        response->set_hit(false);
        response->set_leader_id(current_leader_);
        response->set_success(false);
        done->Run();
        return;
    }

    if (status_ == kCandidate) {
        response->set_hit(false);
        response->set_leader_id("");
        response->set_success(false);
        done->Run();
        return;
    }

    if (status_ == kLeader && in_safe_mode_) {
        LOG(INFO, "leader is still in safe mode");
        response->set_hit(false);
        response->set_leader_id("");
        response->set_success(false);
        done->Run();
        return;
    }

    int64_t now_timestamp = ins_common::timer::get_micros();
    if (members_.size() > 1
        && (now_timestamp - heartbeat_read_timestamp_) > 
              1000 * FLAGS_elect_timeout_min) {
        LOG(DEBUG, "broadcast for read");
        ClientReadAck::Ptr context(new ClientReadAck());
        context->request = request;
        context->response = response;
        context->done = done;
        context->succ_count = 1;//self Get success;
        std::vector<std::string>::iterator it = members_.begin();
        boost::function<void (const ::galaxy::ins::AppendEntriesRequest*,
                              ::galaxy::ins::AppendEntriesResponse*,
                              bool, int) > callback;
        callback = boost::bind(&InsNodeImpl::HeartBeatForReadCallback, this,
                               _1, _2, _3, _4, context);
        for(; it!= members_.end(); it++) { // make sure I am still leader
            if (*it == self_id_) {
                continue;
            }
            InsNode_Stub* stub;
            rpc_client_.GetStub(*it, &stub);
            boost::scoped_ptr<galaxy::ins::InsNode_Stub> stub_guard(stub);
            ::galaxy::ins::AppendEntriesRequest* request = 
                        new ::galaxy::ins::AppendEntriesRequest();
            ::galaxy::ins::AppendEntriesResponse* response =
                        new ::galaxy::ins::AppendEntriesResponse();
            request->set_term(current_term_);
            request->set_leader_id(self_id_);
            request->set_leader_commit_index(commit_index_);
            rpc_client_.AsyncRequest(stub, &InsNode_Stub::AppendEntries, 
                                     request, response, callback, 2, 1);
        }
    } else {
        mu_.Unlock();
        std::string key = request->key();
        Status s;
        std::string value;
        s = data_store_->Get((request->has_user()) ? request->user() :
                                                     StorageManager::anonymous_user,
                             key, &value);
        std::string real_value;
        LogOperation op;
        ParseValue(value, op, real_value);
        if (s == kOk) {
            if (op == kLock) {
                if (IsExpiredSession(real_value)) {
                    response->set_hit(false);
                    response->set_success(true);
                    response->set_leader_id("");
                } else{
                    response->set_hit(true);
                    response->set_success(true);
                    response->set_value(real_value);
                    response->set_leader_id("");
                }
            }
            else {
                response->set_hit(true);
                response->set_success(true);
                response->set_value(real_value);
                //LOG(INFO, "get value: %s", real_value.c_str());
                response->set_leader_id("");
            }
        } else {
            response->set_hit(false);
            response->set_success(true);
            response->set_leader_id("");
        }
        done->Run();
        mu_.Lock();
    }
}

void InsNodeImpl::Delete(::google::protobuf::RpcController* /*controller*/,
                        const ::galaxy::ins::DelRequest* request,
                        ::galaxy::ins::DelResponse* response,
                        ::google::protobuf::Closure* done) {
    MutexLock lock(&mu_);
    if (status_ == kFollower) {
        response->set_success(false);
        response->set_leader_id(current_leader_);
        done->Run();
        return;
    }

    if (status_ == kCandidate) {
        response->set_success(false);
        response->set_leader_id("");
        done->Run();
        return;
    }

    const std::string& key = request->key();
    LOG(DEBUG, "client want delete key :%s", key.c_str());
    LogEntry log_entry;
    if (request->has_user()) {
        log_entry.user = request->user();
    }
    log_entry.key = key;
    log_entry.value = "";
    log_entry.term = current_term_;
    log_entry.op = kDel;
    binlogger_->AppendEntry(log_entry);
    int64_t cur_index = binlogger_->GetLength() - 1;
    ClientAck& ack = client_ack_[cur_index];
    ack.done = done;
    ack.del_response = response;
    replication_cond_->Broadcast();
    if (single_node_mode_) { //single node cluster
        UpdateCommitIndex(binlogger_->GetLength() - 1);
    }
    return;
}

void InsNodeImpl::Put(::google::protobuf::RpcController* /*controller*/,
                      const ::galaxy::ins::PutRequest* request,
                      ::galaxy::ins::PutResponse* response,
                      ::google::protobuf::Closure* done) {
    MutexLock lock(&mu_);
    if (status_ == kFollower) {
        response->set_success(false);
        response->set_leader_id(current_leader_);
        done->Run();
        return;
    }

    if (status_ == kCandidate) {
        response->set_success(false);
        response->set_leader_id("");
        done->Run();
        return;
    }

    const std::string& key = request->key();
    const std::string& value = request->value();
    LOG(DEBUG, "client want put key :%s", key.c_str());
    LogEntry log_entry;
    if (request->has_user()) {
        log_entry.user = request->user();
    }
    log_entry.key = key;
    log_entry.value = value;
    log_entry.term = current_term_;
    log_entry.op = kPut;
    binlogger_->AppendEntry(log_entry);
    int64_t cur_index = binlogger_->GetLength() - 1;
    ClientAck& ack = client_ack_[cur_index];
    ack.done = done;
    ack.response = response;
    replication_cond_->Broadcast();
    if (single_node_mode_) { //single node cluster
        UpdateCommitIndex(binlogger_->GetLength() - 1);
    }
    return;
}

bool InsNodeImpl::LockIsAvailable(const std::string& key,
                                 const std::string& session_id) {
    Status s;
    std::string old_locker_session;
    std::string value;
    LogOperation op;
    s = data_store_->Get(StorageManager::anonymous_user, key, &value);
    ParseValue(value, op, old_locker_session);
    bool lock_is_available = false;
    if (s != kOk) {
        MutexLock lock(&sessions_mu_);
        SessionIDIndex& id_index = sessions_.get<0>();
        SessionIDIndex::iterator self_it = id_index.find(session_id);
        if (self_it != sessions_.end()) {
            lock_is_available = true;
        }
    } else {
        if (op != kLock) {
            lock_is_available = false;
        } else {
            MutexLock lock(&sessions_mu_);
            SessionIDIndex& id_index = sessions_.get<0>();
            SessionIDIndex::iterator old_it = id_index.find(old_locker_session);
            SessionIDIndex::iterator self_it = id_index.find(session_id);
            if (old_it == sessions_.end() //expired session
                && self_it != sessions_.end()) { 
                lock_is_available = true;
            }
        }
    }
    return lock_is_available;
}

void InsNodeImpl::Lock(::google::protobuf::RpcController* /*controller*/,
                       const ::galaxy::ins::LockRequest* request,
                       ::galaxy::ins::LockResponse* response,
                       ::google::protobuf::Closure* done) {
    MutexLock lock(&mu_);
    if (status_ == kFollower) {
        response->set_success(false);
        response->set_leader_id(current_leader_);
        done->Run();
        return;
    }

    if (status_ == kCandidate) {
        response->set_success(false);
        response->set_leader_id("");
        done->Run();
        return;
    }

    if (status_ == kLeader && in_safe_mode_) {
        LOG(INFO, "leader is still in safe mode");
        response->set_leader_id("");
        response->set_success(false);
        done->Run();
        return;
    }

    int64_t tm_now = ins_common::timer::get_micros();
    if (status_ == kLeader && 
        (tm_now - server_start_timestamp_) < FLAGS_session_expire_timeout) {
        LOG(INFO, "leader is still in safe mode for lock");
        response->set_leader_id("");
        response->set_success(false);
        done->Run();
        return;
    }

    const std::string& key = request->key();
    const std::string& session_id = request->session_id();
    LogEntry log_entry;
    if (request->has_user()) {
        log_entry.user = request->user();
    }
    log_entry.key = key;
    log_entry.value = session_id;
    log_entry.term = current_term_;
    log_entry.op = kLock;
    bool lock_is_available = false;
    lock_is_available = LockIsAvailable(key, session_id);
    if (lock_is_available) {
        LOG(INFO, "lock key :%s, session:%s",
                   key.c_str(),
                   session_id.c_str());
        std::string type_and_value;
        type_and_value.append(1, static_cast<char>(kLock));
        type_and_value.append(session_id);
        Status st = data_store_->Put((request->has_user()) ? request->user() :
                                                             StorageManager::anonymous_user,
                                     key, type_and_value);
        assert(st == kOk);
        binlogger_->AppendEntry(log_entry);
        int64_t cur_index = binlogger_->GetLength() - 1;
        ClientAck& ack = client_ack_[cur_index];
        ack.done = done;
        ack.lock_response = response;
        replication_cond_->Broadcast();
        if (single_node_mode_) { //single node cluster
            UpdateCommitIndex(binlogger_->GetLength() - 1);
        }
    } else {
        LOG(DEBUG, "the lock %s is hold by another session",
            key.c_str());
        response->set_leader_id("");
        response->set_success(false);
        done->Run();
    }
    return;
}

void InsNodeImpl::Scan(::google::protobuf::RpcController* /*controller*/,
                       const ::galaxy::ins::ScanRequest* request,
                       ::galaxy::ins::ScanResponse* response,
                       ::google::protobuf::Closure* done) {
    MutexLock lock(&mu_);
    if (status_ == kFollower) {
        response->set_leader_id(current_leader_);
        response->set_success(false);
        done->Run();
        return;
    }

    if (status_ == kCandidate) {
        response->set_leader_id("");
        response->set_success(false);
        done->Run();
        return;
    }

    if (status_ == kLeader && in_safe_mode_) {
        LOG(INFO, "leader is still in safe mode");
        response->set_leader_id("");
        response->set_success(false);
        done->Run();
        return;
    }

    int64_t tm_now = ins_common::timer::get_micros();
    if (status_ == kLeader &&
        (tm_now - server_start_timestamp_) < FLAGS_session_expire_timeout) {
        LOG(INFO, "leader is still in safe mode for scan");
        response->set_leader_id("");
        response->set_success(false);
        done->Run();
        return;
    }

    mu_.Unlock();
    std::string start_key = request->start_key();
    std::string end_key = request->end_key();
    int32_t size_limit = request->size_limit();
    StorageManager::Iterator* it = data_store_->NewIterator((request->has_user()) ?
                request->user() : StorageManager::anonymous_user);
    bool has_more = false;
    int32_t count = 0;
    for (it->Seek(start_key);
         it->Valid() && (it->key() < end_key || end_key.empty());
         it->Next()) {
        if (count > size_limit) {
            has_more = true;
            break;
        }
        if (it->key() == tag_last_applied_index) {
            continue;
        }
        std::string value = it->value();
        std::string real_value;
        LogOperation op;
        ParseValue(value, op, real_value);
        if (op == kLock) {
            if (IsExpiredSession(real_value)) {
                LOG(INFO, "expired value: %s", real_value.c_str());
                continue;
            }
        }
        galaxy::ins::ScanItem* item = response->add_items();
        item->set_key(it->key());
        item->set_value(real_value);
        count ++;
    }

    assert(it->status() == kOk);
    delete it;
    response->set_has_more(has_more);
    response->set_success(true);
    done->Run();

    mu_.Lock();
    return;
}

void InsNodeImpl::KeepAlive(::google::protobuf::RpcController* /*controller*/,
                            const ::galaxy::ins::KeepAliveRequest* request,
                            ::galaxy::ins::KeepAliveResponse* response,
                            ::google::protobuf::Closure* done) {
    {
        MutexLock lock(&mu_);
        if (status_ == kFollower && !request->forward_from_leader()) {
            response->set_success(false);
            response->set_leader_id(current_leader_);
            done->Run();
            return;
        }

        if (status_ == kCandidate && !request->forward_from_leader()) {
            response->set_success(false);
            response->set_leader_id("");
            done->Run();
            return;
        }
    } //end of global mutex
    Session session;
    session.session_id = request->session_id();
    session.last_report_time = ins_common::timer::get_micros();
    session.host_name = request->host_name();
    {
        MutexLock lock(&sessions_mu_);
        SessionIDIndex& id_index = sessions_.get<0>();
        SessionIDIndex::iterator it = id_index.find(session.session_id);
        if (it == sessions_.end()) {
            id_index.insert(session);
        } else {
            id_index.replace(it, session);
        }
    }
    {
        MutexLock lock_sk(&session_locks_mu_);
        session_locks_[session.session_id].clear();
        for (int i = 0; i < request->locks_size(); i++) {
            session_locks_[session.session_id].insert(request->locks(i));
        }
    }
    response->set_success(true);
    response->set_leader_id("");
    LOG(DEBUG, "recv session id: %s", session.session_id.c_str());
    //forward heartbeat of clients
    ForwardKeepAlive(request, response);
    done->Run();
}

void InsNodeImpl::ForwardKeepAlive(const ::galaxy::ins::KeepAliveRequest * request,
                                   ::galaxy::ins::KeepAliveResponse * response) {
    std::vector<std::string> followers;
    {
        MutexLock lock(&mu_);
        if (status_ != kLeader) {
            return;
        }
        std::vector<std::string>::iterator it = members_.begin();
        for(; it!= members_.end(); it++) {
            if (*it == self_id_) {
                continue;
            }
            followers.push_back(*it);
        }
    }
    std::vector<std::string>::iterator it;
    for (it = followers.begin(); it != followers.end(); it++) {
        InsNode_Stub* stub;
        rpc_client_.GetStub(*it, &stub);
        boost::scoped_ptr<galaxy::ins::InsNode_Stub> stub_guard(stub);
        ::galaxy::ins::KeepAliveRequest* forward_request = 
            new ::galaxy::ins::KeepAliveRequest();
        ::galaxy::ins::KeepAliveResponse* forward_response =
            new ::galaxy::ins::KeepAliveResponse();
        forward_request->CopyFrom(*request);
        forward_response->CopyFrom(*response);
        forward_request->set_forward_from_leader(true);
        boost::function<void (const ::galaxy::ins::KeepAliveRequest*,
                        ::galaxy::ins::KeepAliveResponse*,
                        bool, int) > callback;
        callback = boost::bind(&InsNodeImpl::ForwardKeepAliveCallback,
                               this,
                               _1, _2, _3, _4);
        rpc_client_.AsyncRequest(stub, &InsNode_Stub::KeepAlive, 
                                 forward_request, 
                                 forward_response, callback, 2, 1);
    }
}

void InsNodeImpl::RemoveExpiredSessions() {
    int64_t cur_term;
    NodeStatus cur_status;
    {
        MutexLock lock(&mu_);
        cur_term = current_term_;
        if (stop_) {
            return;
        }
        cur_status = status_;
    }

    std::vector<std::string> expired_sessions;
 
    {
        MutexLock lock_session(&sessions_mu_);
        SessionTimeIndex& time_index = sessions_.get<1>();
        if (!sessions_.empty()) {
            int64_t expired_line = ins_common::timer::get_micros() -
                                   FLAGS_session_expire_timeout;
            SessionTimeIndex::iterator it = time_index.lower_bound(expired_line);
            if (it != time_index.begin()) {
                LOG(INFO, "remove expired session");
                for (SessionTimeIndex::iterator dd = time_index.begin();
                     dd != it; dd++) {
                    expired_sessions.push_back(dd->session_id);
                    LOG(INFO, "remove session_id %s", dd->session_id.c_str());
                }
                time_index.erase(time_index.begin(), it);
            }
        }
    }

    {
        MutexLock lock_watch(&watch_mu_);
        std::vector<std::string>::iterator it = expired_sessions.begin();
        for ( ; it != expired_sessions.end(); it++){
            RemoveEventBySession(*it);
        }
    }

    std::vector<std::pair<std::string, std::string> > unlock_keys;

    {
        MutexLock lock_sk(&session_locks_mu_);
        std::vector<std::string>::iterator it = expired_sessions.begin();
        for ( ; it != expired_sessions.end(); it++){
            std::string session_id = *it;
            if (session_locks_.find(session_id) != session_locks_.end()) {
                std::set<std::string>& keys = session_locks_[session_id];
                std::set<std::string>::iterator jt = keys.begin();
                for(; jt != keys.end(); jt++) {
                    unlock_keys.push_back(std::make_pair(*jt, session_id));
                }
                session_locks_.erase(session_id);
            }
        }      
    }

    if (cur_status == kLeader) {
        for (size_t i = 0; i < unlock_keys.size(); i++){
            std::string key = unlock_keys[i].first;
            std::string session_id = unlock_keys[i].second;
            LogEntry log_entry;
            log_entry.key = key;
            log_entry.value = session_id;
            log_entry.term = cur_term;
            log_entry.op = kUnLock;
            binlogger_->AppendEntry(log_entry);   
        }
        if (single_node_mode_) { //single node cluster
            MutexLock lock(&mu_);
            UpdateCommitIndex(binlogger_->GetLength() - 1);
        }
    }
    session_checker_.DelayTask(2000, 
        boost::bind(&InsNodeImpl::RemoveExpiredSessions, this)
    );
}

void InsNodeImpl::ParseValue(const std::string& value,
                LogOperation& op, 
                std::string& real_value) {
    if (value.size() >= 1) {
        op = static_cast<LogOperation>(value[0]);
        real_value = value.substr(1);
    }
}

bool InsNodeImpl::IsExpiredSession(const std::string& session_id) {
    bool expired_session = false;
    {
        MutexLock lock(&sessions_mu_);
        SessionIDIndex& id_index = sessions_.get<0>();
        SessionIDIndex::iterator old_it = id_index.find(session_id);
        if (old_it == sessions_.end()) { 
            expired_session = true;
        }
    }
    return expired_session;
}

void InsNodeImpl::TriggerEventWithParent(const std::string& key,
                                         const std::string& value,
                                         bool deleted) {
    std::string::size_type tail_index = key.rfind("/");
    std::string parent_key = "";
    if (tail_index != std::string::npos) {
        parent_key = key.substr(0, tail_index);
    }
    TriggerEvent(key, key, value, deleted);
    if (!parent_key.empty()) {
        TriggerEvent(parent_key, key, value, deleted);
    }
}

void InsNodeImpl::TriggerEvent(const std::string& watch_key,
                               const std::string& key,
                               const std::string& value,
                               bool deleted) {
    MutexLock lock(&watch_mu_);
    WatchEventKeyIndex& key_idx = watch_events_.get<0>();
    WatchEventKeyIndex::iterator it_start = key_idx.lower_bound(watch_key);
    if (it_start != key_idx.end() 
        && it_start->key == watch_key) {
        WatchEventKeyIndex::iterator it_end = key_idx.upper_bound(watch_key);
        int event_count = 0;
        for (WatchEventKeyIndex::iterator it = it_start;
             it != it_end; it++) {
            it->ack->response->set_watch_key(watch_key);
            it->ack->response->set_key(key);
            it->ack->response->set_value(value);
            it->ack->response->set_deleted(deleted);
            it->ack->response->set_success(true);
            it->ack->response->set_leader_id("");
            event_count++;
        }
        key_idx.erase(it_start, it_end);
        LOG(INFO, "trigger #%d watch event: %s",
                  event_count, key.c_str());
    } else {
        LOG(DEBUG, "watch list: no such key : %s", key.c_str());
    }
}

void InsNodeImpl::RemoveEventBySessionAndKey(const std::string& session_id,
                                             const std::string& key) {
    watch_mu_.AssertHeld();
    WatchEventSessionIndex& session_idx = watch_events_.get<1>();
    WatchEventSessionIndex::iterator it_start = session_idx.lower_bound(session_id);
    if (it_start != session_idx.end() 
        && it_start->session_id == session_id) {
        WatchEventSessionIndex::iterator it_end = 
              session_idx.upper_bound(session_id);
        for (WatchEventSessionIndex::iterator it = it_start;
             it != it_end; ) {
            if (it->key == key) {
                 LOG(DEBUG, "remove watch event: %s on %s",
                     it->key.c_str(), it->session_id.c_str());
                it->ack->response->set_canceled(true);
                it = session_idx.erase(it);
            } else {
                it++;
            }
        }
    }
}


void InsNodeImpl::TriggerEventBySessionAndKey(const std::string& session_id,
                                              const std::string& key,
                                              const std::string& value,
                                              bool deleted) {
    MutexLock lock(&watch_mu_);
    WatchEventSessionIndex& session_idx = watch_events_.get<1>();
    WatchEventSessionIndex::iterator it_start = session_idx.lower_bound(session_id);
    if (it_start != session_idx.end() 
        && it_start->session_id == session_id) {
        WatchEventSessionIndex::iterator it_end = 
              session_idx.upper_bound(session_id);
        for (WatchEventSessionIndex::iterator it = it_start;
             it != it_end; ) {
            if (it->key == key) {
                LOG(INFO, "trigger watch event: %s on %s",
                           it->key.c_str(), it->session_id.c_str());
                it->ack->response->set_watch_key(key);
                it->ack->response->set_key(key);
                it->ack->response->set_value(value);
                it->ack->response->set_deleted(deleted);
                it->ack->response->set_success(true);
                it->ack->response->set_leader_id("");
                it = session_idx.erase(it);
            } else {
                it++;
            }
        }
    }
}

void InsNodeImpl::RemoveEventBySession(const std::string& session_id) {
    watch_mu_.AssertHeld();
    WatchEventSessionIndex& session_idx = watch_events_.get<1>();
    WatchEventSessionIndex::iterator it_start = session_idx.lower_bound(session_id);
    if (it_start != session_idx.end() 
        && it_start->session_id == session_id) {
        WatchEventSessionIndex::iterator it_end = 
              session_idx.upper_bound(session_id);
        for (WatchEventSessionIndex::iterator it = it_start;
             it != it_end; it++) {
            LOG(DEBUG, "remove watch event: %s on %s",
                it->key.c_str(), it->session_id.c_str());
        }
        session_idx.erase(it_start, it_end);
    }
}

void InsNodeImpl::Watch(::google::protobuf::RpcController* /*controller*/,
                        const ::galaxy::ins::WatchRequest* request,
                        ::galaxy::ins::WatchResponse* response,
                        ::google::protobuf::Closure* done) {
    {
        MutexLock lock(&mu_);
        if (status_ == kFollower) {
            response->set_success(false);
            response->set_leader_id(current_leader_);
            done->Run();
            return;
        }

        if (status_ == kCandidate) {
            response->set_success(false);
            response->set_leader_id("");
            done->Run();
            return;
        } 
    }
    
    WatchAck::Ptr ack_obj(new WatchAck(response, done));
    
    std::string key = request->key();
    {
        MutexLock lock(&watch_mu_);
        WatchEvent watch_event;
        watch_event.key = key;
        watch_event.session_id = request->session_id();
        watch_event.ack = ack_obj;
        RemoveEventBySessionAndKey(watch_event.session_id, watch_event.key);
        watch_events_.insert(watch_event);
    }
    int64_t tm_now = ins_common::timer::get_micros(); 
    if (tm_now - server_start_timestamp_ > FLAGS_session_expire_timeout) {
        Status s;
        std::string raw_value;
        s = data_store_->Get((request->has_user()) ? request->user() :
                                                     StorageManager::anonymous_user,
                             key, &raw_value);
        bool key_exist = (s == kOk);
        std::string real_value;
        LogOperation op;
        ParseValue(raw_value, op, real_value);
        if (real_value != request->old_value() || 
            key_exist != request->key_exist()) {
            LOG(INFO, "key:%s, new_v: %s, old_v:%s", 
                key.c_str(), real_value.c_str(), request->old_value().c_str());
            TriggerEventBySessionAndKey(request->session_id(),
                                        key, real_value, (s == kNotFound));
        } else if (op == kLock && IsExpiredSession(real_value)) {
            LOG(INFO, "key(lock):%s, new_v: %s, old_v:%s", 
                key.c_str(), real_value.c_str(), request->old_value().c_str());
            TriggerEventBySessionAndKey(request->session_id(),
                                        key, "", true);
        }
    }
}

void InsNodeImpl::UnLock(::google::protobuf::RpcController* /*controller*/,
                         const ::galaxy::ins::UnLockRequest* request,
                         ::galaxy::ins::UnLockResponse* response,
                         ::google::protobuf::Closure* done) {
    MutexLock lock(&mu_);
    if (status_ == kFollower) {
        response->set_success(false);
        response->set_leader_id(current_leader_);
        done->Run();
        return;
    }

    if (status_ == kCandidate) {
        response->set_success(false);
        response->set_leader_id("");
        done->Run();
        return;
    }

    const std::string& key = request->key();
    const std::string& session_id = request->session_id();
    LOG(DEBUG, "client want unlock key :%s", key.c_str());
    LogEntry log_entry;
    if (request->has_user()) {
        log_entry.user = request->user();
    }
    log_entry.key = key;
    log_entry.value = session_id;
    log_entry.term = current_term_;
    log_entry.op = kUnLock;
    binlogger_->AppendEntry(log_entry);
    int64_t cur_index = binlogger_->GetLength() - 1;
    ClientAck& ack = client_ack_[cur_index];
    ack.done = done;
    ack.unlock_response = response;
    replication_cond_->Broadcast();
    if (single_node_mode_) { //single node cluster
        UpdateCommitIndex(binlogger_->GetLength() - 1);
    }
    return;
}

void InsNodeImpl::Login(::google::protobuf::RpcController* /*controller*/,
                        const ::galaxy::ins::LoginRequest* request,
                        ::galaxy::ins::LoginResponse* response,
                        ::google::protobuf::Closure* done) {
    MutexLock lock(&mu_);
    if (status_ == kFollower) {
        response->set_status(kError);
        response->set_leader_id(current_leader_);
        done->Run();
        return;
    }

    if (status_ == kCandidate) {
        response->set_status(kError);
        response->set_leader_id("");
        done->Run();
        return;
    }

    // TODO DO something here
}

void InsNodeImpl::Logout(::google::protobuf::RpcController* /*controller*/,
                         const ::galaxy::ins::LogoutRequest* request,
                         ::galaxy::ins::LogoutResponse* response,
                         ::google::protobuf::Closure* done) {
    MutexLock lock(&mu_);
    if (status_ == kFollower) {
        response->set_success(false);
        response->set_leader_id(current_leader_);
        done->Run();
        return;
    }

    if (status_ == kCandidate) {
        response->set_success(false);
        response->set_leader_id("");
        done->Run();
        return;
    }

    // TODO Do something here
}

void InsNodeImpl::DelBinlog(int64_t index) {
    LOG(INFO, "delete binlog [%ld]", index);
    bool ret = binlogger_->RemoveSlot(index);
    if (ret) {
        binlog_cleaner_.DelayTask(10, //10ms delay
            boost::bind(&InsNodeImpl::DelBinlog, this, index -1 )
        );
    } else {
        LOG(INFO, "delete binlog stop at [%ld]", index);
        return;
    }
}

void InsNodeImpl::CleanBinlog(::google::protobuf::RpcController* /*controller*/,
                              const ::galaxy::ins::CleanBinlogRequest* request,
                              ::galaxy::ins::CleanBinlogResponse* response,
                              ::google::protobuf::Closure* done) {
    int64_t del_end_index = request->end_index();
    {
        MutexLock lock(&mu_);
        if (last_applied_index_ < del_end_index) {
            response->set_success(false);
            LOG(FATAL, "del log  %ld > %ld is unsafe", 
                del_end_index, last_applied_index_);
            done->Run();
            return;
        }
    }
    binlog_cleaner_.AddTask(
        boost::bind(&InsNodeImpl::DelBinlog, this, del_end_index -1 )
    );
    response->set_success(true);
    done->Run();
}

} //namespace ins
} //namespace galaxy

