#include "ins_sdk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <iostream>
#include <boost/algorithm/string.hpp>
#include <gflags/gflags.h>
#include "common/logging.h"

DECLARE_string(cluster_members);
DECLARE_string(ins_cmd);
DECLARE_string(ins_key);
DECLARE_string(ins_value);

using namespace galaxy::ins::sdk;

int main(int argc, char* argv[]) {
    google::ParseCommandLineFlags(&argc, &argv, true);
    std::vector<std::string> members;
    boost::split(members, FLAGS_cluster_members,
                 boost::is_any_of(","), boost::token_compress_on);
    InsSDK sdk(members);
    galaxy::ins::sdk::SDKError ins_err;
    if (FLAGS_ins_cmd == "show") {
        std::vector<ClusterNodeInfo> cluster_info;
        sdk.ShowCluster(&cluster_info);
        std::vector<ClusterNodeInfo>::iterator it;
        std::cout << "server_addr \t term \t last_log_index \t last_log_term"
                  << std::endl
                  << "-------------------------------------------------------"
                  << std::endl; 
        for(it = cluster_info.begin(); it != cluster_info.end(); it++ ) {
            std::string s_status = InsSDK::StatusToString(it->status);
            std::cout << it->server_id << "\t" << s_status 
                      << "\t" << it->term
                      << "\t" << it->last_log_index
                      << "\t" << it->last_log_term
                      << std::endl;
        }
    }
    if (FLAGS_ins_cmd == "put") {
        std::string key = FLAGS_ins_key;
        std::string value = FLAGS_ins_value;
        LOG(INFO, "key: %s, value: %s", key.c_str(), value.c_str());
        if (sdk.Put(key, value, &ins_err)) {
            LOG(INFO, "put success");
        } else {
            LOG(FATAL, "put failed");
        }
    }

    if (FLAGS_ins_cmd == "delete") {
        std::string key = FLAGS_ins_key;
        LOG(INFO, "key: %s", key.c_str());
        if (sdk.Delete(key, &ins_err)) {
            LOG(INFO, "delete success");
        } else {
            LOG(FATAL, "delete failed");
        }
    }

    if (FLAGS_ins_cmd == "get") {
        std::string key = FLAGS_ins_key;
        std::string value;
        LOG(INFO, "key: %s", key.c_str());
        if (sdk.Get(key, &value, &ins_err)) {
            LOG(INFO, "get success");
            if (ins_err == kOK) {
                printf("value: %s\n", value.c_str());
            } else {
                printf("NOT FOUND\n");
            }
        } else {
            LOG(FATAL, "get failed");
        }
    }

    return 0;
}
