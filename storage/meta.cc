#include "meta.h"

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <boost/filesystem.hpp>

namespace galaxy {
namespace ins {

namespace bf = boost::filesystem;
const std::string term_file_name = "term.data";
const std::string vote_file_name = "vote.data";

Meta::Meta(const std::string& data_dir) : data_dir_(data_dir),
										  term_file_(NULL),
										  vote_file_(NULL) {
	if (!bf::exists(data_dir)) {
		bf::create_directories(data_dir);
	}
	term_file_ = fopen((data_dir+"/"+term_file_name).c_str(), "a+");
	vote_file_ = fopen((data_dir+"/"+vote_file_name).c_str(), "a+");
	assert(term_file_);
	assert(vote_file_);
}

Meta::~Meta() {
	fclose(term_file_);
	fclose(vote_file_);
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
	while(fscanf(vote_file_, "%ld %s", &term, server_id) == 2) {
		voted_for[term] = std::string(server_id);
	}
}

void Meta::WriteCurrentTerm(int64_t current_term) {
	fprintf(term_file_, "%ld\n", current_term);
	fflush(term_file_);	
}

void Meta::WriteVotedFor(int64_t term, const std::string& server_id) {
	fprintf(vote_file_, "%ld %s\n", term, server_id.c_str());
	fflush(vote_file_);
}

} //namespace ins
} //namespace galaxy


