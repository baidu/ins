#include "ins_sdk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <algorithm>
#include <vector>
#include <iostream>
#include <gflags/gflags.h>
#include "common/logging.h"

DECLARE_string(ins_cmd);
DECLARE_string(ins_key);
DECLARE_string(ins_value);
DECLARE_string(ins_start_key);
DECLARE_string(ins_end_key);

using namespace galaxy::ins::sdk;

void my_watch_callback(const WatchParam& param, SDKError error) {
    printf("key: %s\n", param.key.c_str());
    printf("new value: %s\n", param.new_value.c_str());
    printf("old value: %s\n", param.old_value.c_str());
    printf("now has key: %s\n", param.now_has_key? "true":"false");
    printf("old has key: %s\n", param.old_has_key? "true":"false");
    printf("error code: %d\n", static_cast<int>(error));
    bool* done = reinterpret_cast<bool*>(param.context);
    if (done) {
        *done = true;
    }
    (void)error;
}

int main(int argc, char* argv[]) {
    std::vector<std::string> members;
    InsSDK::ParseFlagFromArgs(argc, argv, &members);
    InsSDK sdk(members);
    char cli_header[2048] = {'\0'};
    snprintf(cli_header, sizeof(cli_header), 
             "%-*s\t%-*s\t%-*s\t%-*s\t%-*s\t%-*s", 
             35, "server node",
             10, "role",
             15, "term",
             15, "last_log_index",
             15, "last_log_term",
             15, "commit_index");
    std::string header_line;
    header_line.resize(strlen(cli_header) + 8);
    std::fill(header_line.begin(), header_line.end(), '-');
    galaxy::ins::sdk::SDKError ins_err;
    if (FLAGS_ins_cmd == "show") {
        std::vector<ClusterNodeInfo> cluster_info;
        sdk.ShowCluster(&cluster_info);
        std::vector<ClusterNodeInfo>::iterator it;
        std::cout << cli_header
                  << std::endl
                  << header_line
                  << std::endl; 
        for(it = cluster_info.begin(); it != cluster_info.end(); it++ ) {
            std::string s_status = InsSDK::StatusToString(it->status);
            char raw_info[1024] = {'\0'};
            snprintf(raw_info, sizeof(raw_info), 
                     "%-*s\t%-*s\t%-*ld\t%-*ld\t%-*ld\t%-*ld", 
                     35, it->server_id.c_str(),
                     10, s_status.c_str(),
                     15, it->term,
                     15, it->last_log_index,
                     15, it->last_log_term,
                     15, it->commit_index);
            std::cout << raw_info
                      << std::endl;
        }
    }
    if (FLAGS_ins_cmd == "put") {
        std::string key = FLAGS_ins_key;
        std::string value = FLAGS_ins_value;
        LOG(DEBUG, "key: %s, value: %s", key.c_str(), value.c_str());
        if (sdk.Put(key, value, &ins_err)) {
            LOG(DEBUG, "put success");
        } else {
            LOG(FATAL, "put failed");
        }
    }

    if (FLAGS_ins_cmd == "delete") {
        std::string key = FLAGS_ins_key;
        LOG(DEBUG, "key: %s", key.c_str());
        if (sdk.Delete(key, &ins_err)) {
            LOG(DEBUG, "delete success");
        } else {
            LOG(FATAL, "delete failed");
        }
    }

    if (FLAGS_ins_cmd == "get") {
        std::string key = FLAGS_ins_key;
        std::string value;
        LOG(DEBUG, "key: %s", key.c_str());
        if (sdk.Get(key, &value, &ins_err)) {
            LOG(DEBUG, "get success");
            if (ins_err == kOK) {
                printf("value: %s\n", value.c_str());
            } else {
                printf("NOT FOUND\n");
            }
        } else {
            LOG(FATAL, "get failed");
        }
    }

    if (FLAGS_ins_cmd == "scan") {
        std::string start_key = FLAGS_ins_start_key;
        std::string end_key = FLAGS_ins_end_key;
        LOG(INFO, "scan: [%s, %s)", start_key.c_str(), end_key.c_str());
        ScanResult* result = sdk.Scan(start_key, end_key);
        int i = 0;
        while (!result->Done()) {
            printf("[%d]\t%s -> %s\n", ++i,
                   result->Key().c_str(), result->Value().c_str());
            result->Next();
        }
        delete result;
    }

    if (FLAGS_ins_cmd == "watch") {
        std::string key = FLAGS_ins_key;
        SDKError error;
        bool done = false;
        bool ret = sdk.Watch(key, my_watch_callback, &done, &error);
        if (!ret) {
            fprintf(stderr, "rpc error: %d", static_cast<int>(error));
            return 1;
        }
        while (!done) {
            sleep(1);
        }
    }

    if (FLAGS_ins_cmd == "lock") {
        std::string key = FLAGS_ins_key;
        SDKError error;
        bool ret = sdk.Lock(key, &error);
        if (!ret) {
            fprintf(stderr, "lock error: %d", static_cast<int>(error));
            return 1;
        }
        fprintf(stderr, "lock successful on %s\n", key.c_str());
        fprintf(stderr, "Press any key to release the lock.\n");
        getchar();
    }
    return 0;
}
