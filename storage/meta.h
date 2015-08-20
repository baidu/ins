#ifndef GALAXY_INS_META_H_
#define GALAXY_INS_META_H_
#include <stdio.h>
#include <string>
#include <map>
#include <stdint.h>
#include "proto/ins_node.pb.h"

namespace galaxy {
namespace ins {

class UserManager;

class Meta {
public:
    Meta(const std::string& data_dir);
    ~Meta();
    int64_t ReadCurrentTerm();
    void ReadVotedFor(std::map<int64_t, std::string>& voted_for);
    UserInfo ReadRootInfo();
    void WriteCurrentTerm(int64_t term); 
    void WriteVotedFor(int64_t term, const std::string& server_id);
    void WriteRootInfo(const UserInfo& root);
private:
    std::string data_dir_;
    FILE* term_file_;
    FILE* vote_file_;
    FILE* root_file_;
};

} //namespace ins
} //namespace galaxy
#endif
