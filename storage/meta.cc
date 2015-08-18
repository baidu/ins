#include "meta.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>
#include "common/logging.h"
#include "utils.h"
#include "server/user_manage.h"

namespace galaxy {
namespace ins {

const std::string term_file_name = "term.data";
const std::string vote_file_name = "vote.data";
const std::string user_file_name = "user.data";

Meta::Meta(const std::string& data_dir) : data_dir_(data_dir),
                                          term_file_(NULL),
                                          vote_file_(NULL),
                                          user_file_(NULL) {
    bool ok = ins_common::Mkdirs(data_dir.c_str());
    if (!ok) {
        LOG(FATAL, "failed to create dir :%s", data_dir.c_str());
        abort();
    }
    term_file_ = fopen((data_dir + "/" + term_file_name).c_str(), "a+");
    vote_file_ = fopen((data_dir + "/" + vote_file_name).c_str(), "a+");
    user_file_ = fopen((data_dir + "/" + user_file_name).c_str(), "a+");
    assert(term_file_);
    assert(vote_file_);
    assert(user_file_);
}

Meta::~Meta() {
    fclose(term_file_);
    fclose(vote_file_);
    fclose(user_file_);
}

int64_t Meta::ReadCurrentTerm() {
    int64_t cur_term = 0, tmp = 0;
    while(fscanf(term_file_, "%ld", &tmp) == 1) {
        cur_term = tmp;
    }
    return cur_term;
}

void Meta::ReadVotedFor(std::map<int64_t, std::string>& voted_for) {
    voted_for.clear();
    int64_t term = 0;
    char server_id[1024] = {'\0'};
    int64_t last_term;
    std::string last_vote_for;
    while(fscanf(vote_file_, "%ld %1023s", &term, server_id) == 2) {
        last_term = term;
        last_vote_for = server_id;
    }
    if (!last_vote_for.empty()) {
        voted_for[last_term] = std::string(last_vote_for);
    }
}

void Meta::ReadUserList(UserManager* manager) {
    manager->logged_users_.clear();
    manager->user_list_.clear();
    char buf[1024] = {'\0'};
    // TODO Need fseek here ?
    while (fgets(buf, 1023, user_file_) != NULL) {
        char *p;
        for (p = buf; *p != '\t'; ++p);
        *p++ = 0;
        std::string username = buf;
        manager->user_list_[username].set_username(username);
        manager->user_list_[username].set_passwd(p);
    }
}

void Meta::WriteCurrentTerm(int64_t current_term) {
    fprintf(term_file_, "%ld\n", current_term);
    if (fflush(term_file_) != 0) {
        LOG(FATAL, "Meta::WriteCurrentTerm failed, term:%ld", current_term);
        abort();
    }
}

void Meta::WriteVotedFor(int64_t term, const std::string& server_id) {
    fprintf(vote_file_, "%ld %s\n", term, server_id.c_str());
    if (fflush(vote_file_) != 0) {
        LOG(FATAL, "Meta::WriteVotedFor failed, term:%ld, server_id:%s",
            term, server_id.c_str());
        abort();
    }
}

void Meta::WriteUserList(const UserInfo& user) {
    fprintf(user_file_, "%s\t%s\n", user.username().c_str(), user.passwd().c_str());
    if (fflush(user_file_) != 0) {
        LOG(FATAL, "Meta::WriteUserList failed, username:%s",
            user.username().c_str());
        abort();
    }
}

} //namespace ins
} //namespace galaxy

