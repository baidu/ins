#include "ins_sdk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string>
#include <algorithm>
#include <vector>
#include <iostream>
#include <gflags/gflags.h>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include "common/logging.h"

DECLARE_string(ins_cmd);
DECLARE_string(ins_key);
DECLARE_string(ins_value);
DECLARE_string(ins_start_key);
DECLARE_string(ins_end_key);
DECLARE_string(ins_rm_binlog_server_id);
DECLARE_int64(ins_rm_binlog_index);

using namespace galaxy::ins::sdk;

void my_watch_callback(const WatchParam& param, SDKError error) {
    printf("key: %s\n", param.key.c_str());
    printf("value: %s\n", param.value.c_str());
    printf("deleted: %s\n", param.deleted ?"true":"false");
    printf("error code: %d\n", static_cast<int>(error));
    bool* done = reinterpret_cast<bool*>(param.context);
    if (done) {
        *done = true;
    }
}

void session_timeout_callback(void* /*context*/) {
    printf("session timeout\n");
    exit(1);
}

void reset_flags() {
    static char buf[1024];
    printf("galaxy ins> ");
    fgets(buf, 1024, stdin);
    std::vector<std::string> commands;
    boost::split(commands, buf, boost::is_any_of(" \t\n"), boost::token_compress_on);
    for (int i = commands.size(); i < 3; ++i) {
        commands[i] = "";
    }
    FLAGS_ins_cmd = commands[0];
    if (FLAGS_ins_cmd == "delete" || FLAGS_ins_cmd == "get"
        || FLAGS_ins_cmd == "watch") {
        FLAGS_ins_key = commands[1];
    } else if (FLAGS_ins_cmd == "put" || FLAGS_ins_cmd == "login"
               || FLAGS_ins_cmd == "register") {
        FLAGS_ins_key = commands[1];
        FLAGS_ins_value = commands[2];
    } else if (FLAGS_ins_cmd == "scan") {
        FLAGS_ins_start_key = commands[1];
        FLAGS_ins_end_key = commands[2];
    } else if (FLAGS_ins_cmd == "clean") {
        FLAGS_ins_rm_binlog_server_id = commands[1];
        FLAGS_ins_rm_binlog_index = boost::lexical_cast<int>(commands[2]);
    } else {
        // "show" command - do nothing
    }
}

int main(int argc, char* argv[]) {
    std::vector<std::string> members;
    InsSDK::ParseFlagFromArgs(argc, argv, &members);
    InsSDK sdk(members);
    char cli_header[2048] = {'\0'};
    snprintf(cli_header, sizeof(cli_header), 
             "%-*s\t%-*s\t%-*s\t%-*s\t%-*s\t%-*s\t%-*s", 
             35, "server node",
             10, "role",
             15, "term",
             15, "last_log_index",
             15, "last_log_term",
             15, "commit_index",
             15, "last_applied");
    std::string header_line;
    header_line.resize(strlen(cli_header) + 8);
    std::fill(header_line.begin(), header_line.end(), '-');
    galaxy::ins::sdk::SDKError ins_err;
    bool is_logged = false;
    do {
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
                         "%-*s\t%-*s\t%-*ld\t%-*ld\t%-*ld\t%-*ld\t%-*ld",
                         35, it->server_id.c_str(),
                         10, s_status.c_str(),
                         15, it->term,
                         15, it->last_log_index,
                         15, it->last_log_term,
                         15, it->commit_index,
                         15, it->last_applied);
                std::cout << raw_info
                          << std::endl;
            }
        } else if (FLAGS_ins_cmd == "put") {
            std::string key = FLAGS_ins_key;
            std::string value = FLAGS_ins_value;
            LOG(DEBUG, "key: %s, value: %s", key.c_str(), value.c_str());
            if (sdk.Put(key, value, &ins_err)) {
                LOG(DEBUG, "put success");
            } else {
                LOG(FATAL, "put failed");
            }
        } else if (FLAGS_ins_cmd == "delete") {
            std::string key = FLAGS_ins_key;
            LOG(DEBUG, "key: %s", key.c_str());
            if (sdk.Delete(key, &ins_err)) {
                LOG(DEBUG, "delete success");
            } else {
                LOG(FATAL, "delete failed");
            }
        } else if (FLAGS_ins_cmd == "get") {
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
        } else if (FLAGS_ins_cmd == "scan") {
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
        } else if (FLAGS_ins_cmd == "watch") {
            std::string key = FLAGS_ins_key;
            SDKError error;
            bool done = false;
            bool ret = sdk.Watch(key, my_watch_callback, &done, &error);
            if (!ret) {
                fprintf(stderr, "rpc error: %d", static_cast<int>(error));
                return 1;
            }
            sdk.RegisterSessionTimeout(session_timeout_callback, NULL);
            while (!done) {
                sleep(1);
            }
            fprintf(stderr, "done\n");
        } else if (FLAGS_ins_cmd == "lock") {
            std::string key = FLAGS_ins_key;
            SDKError error;
            bool ret = sdk.Lock(key, &error);
            if (!ret) {
                fprintf(stderr, "lock error: %d", static_cast<int>(error));
                return 1;
            }
            sdk.RegisterSessionTimeout(session_timeout_callback, NULL);
            fprintf(stderr, "lock successful on %s\n", key.c_str());
            fprintf(stderr, "Press any key to release the lock.\n");
            getchar();
            ret = sdk.UnLock(key, &error);
            if (ret) {
                fprintf(stderr, "unlock successful on %s\n", key.c_str());
            }
        } else if (FLAGS_ins_cmd == "clean") {
            std::string server_id = FLAGS_ins_rm_binlog_server_id;
            int64_t end_index = FLAGS_ins_rm_binlog_index;
            SDKError error;
            bool ret = sdk.CleanBinlog(server_id, end_index, &error);
            if (!ret) {
                fprintf(stderr, "clean fail: %d\n", static_cast<int>(error));
                return 1;
            }
            fprintf(stderr, "clean ok\n");
        } else if (FLAGS_ins_cmd == "login") {
            std::string username = FLAGS_ins_key;
            std::string password = FLAGS_ins_value;
            LOG(DEBUG, "user %s login", username.c_str());
            if (sdk.Login(username, password, &ins_err)) {
                LOG(DEBUG, "login finished");
                switch(ins_err) {
                case kOK: printf("login success\nperform actions for user %s now\n",
                                  username.c_str());
                          is_logged = true; break;
                case kUnknownUser: printf("user doesn't exist\n"); break;
                case kUserLogged: printf("user has logged in\n"); break;
                case kPasswordError: printf("wrong password"); break;
                default: break;
                }
            } else {
                LOG(FATAL, "login failed");
            }
        } else if (FLAGS_ins_cmd == "logout") {
            LOG(DEBUG, "user logout");
            if (sdk.Logout(&ins_err)) {
                LOG(DEBUG, "logout success");
                if (ins_err == kOK) {
                    printf("logout success\n");
                } else {
                    printf("user has logged out\n");
                }
                is_logged = false;
            } else {
                LOG(FATAL, "logout failed");
            }
        } else if (FLAGS_ins_cmd == "register") {
            std::string username = FLAGS_ins_key;
            std::string password = FLAGS_ins_value;
            LOG(DEBUG, "register new user %s", username.c_str());
            if (sdk.Register(username, password, &ins_err)) {
                LOG(DEBUG, "register finished");
                if (ins_err == kOK) {
                    printf("register success\n");
                } else {
                    printf("username has been registered\n");
                }
            } else {
                LOG(FATAL, "register failed");
            }
        } else if (is_logged) {
            printf("not a valid command for logged user, "
                    "you may want to logout by `logout' command");
        } else {
            // Just exit
        }
        if (is_logged) {
            reset_flags();
        }
    } while(is_logged);

    return 0;
}
