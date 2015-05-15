#include "ins_sdk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <iostream>
#include <boost/algorithm/string.hpp>
#include <gflags/gflags.h>

DECLARE_string(cluster_members);
DECLARE_string(ins_cmd);

using namespace galaxy::ins::sdk;

int main(int argc, char* argv[]) {
    google::ParseCommandLineFlags(&argc, &argv, true);
    std::vector<std::string> members;
    boost::split(members, FLAGS_cluster_members,
                 boost::is_any_of(","), boost::token_compress_on);
    InsSDK sdk(members);
    if (FLAGS_ins_cmd == "show") {
        std::vector<ClusterNodeInfo> cluster_info;
        sdk.ShowCluster(&cluster_info);
        std::vector<ClusterNodeInfo>::iterator it;
        for(it = cluster_info.begin(); it != cluster_info.end(); it++ ) {
            std::string s_status = InsSDK::StatusToString(it->status);
            std::cout << it->server_id << "\t" << s_status 
                      << "\t" << it->term
                      << "\t" << it->last_log_index
                      << "\t" << it->last_log_term
                      << std::endl;
        }
    }
    return 0;
}
