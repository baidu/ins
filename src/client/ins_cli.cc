#include "sdk/ins_sdk.h"

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
#include "common/tprinter.h"

DECLARE_string(ins_cmd);
DECLARE_string(ins_key);
DECLARE_string(ins_value);
DECLARE_string(ins_start_key);
DECLARE_string(ins_end_key);
DECLARE_string(ins_rm_binlog_server_id);
DECLARE_int64(ins_rm_binlog_index);

using namespace galaxy::ins::sdk;

void my_watch_callback(const WatchParam& param, SDKError error) {
    if (error == kUnknownUser) {
        fprintf(stderr, "previous login may expired, please logout\n");
    } else {
        printf("key: %s\n", param.key.c_str());
        printf("value: %s\n", param.value.c_str());
        printf("deleted: %s\n", param.deleted ?"true":"false");
        printf("error code: %d\n", static_cast<int>(error));
    }
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
        commands.push_back("");
    }
    FLAGS_ins_cmd = commands[0];
    if (FLAGS_ins_cmd == "delete" || FLAGS_ins_cmd == "get"
        || FLAGS_ins_cmd == "watch" || FLAGS_ins_cmd == "lock") {
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
    galaxy::ins::sdk::SDKError ins_err;
    bool is_logged = false;
    do {
        if (FLAGS_ins_cmd == "show") {
            TPrinter cprinter(7);
            cprinter.AddRow(7, "server node", "role", "term", "last_log_index",
                           "last_log_term", "commit_index", "last_applied");
            std::vector<ClusterNodeInfo> cluster_info;
            sdk.ShowCluster(&cluster_info);
            std::vector<ClusterNodeInfo>::iterator it;
            for(it = cluster_info.begin(); it != cluster_info.end(); it++ ) {
                std::string s_status = InsSDK::StatusToString(it->status);
                cprinter.AddRow(7, it->server_id.c_str(), s_status.c_str(),
                        boost::lexical_cast<std::string>(it->term).c_str(),
                        boost::lexical_cast<std::string>(it->last_log_index).c_str(),
                        boost::lexical_cast<std::string>(it->last_log_term).c_str(),
                        boost::lexical_cast<std::string>(it->commit_index).c_str(),
                        boost::lexical_cast<std::string>(it->last_applied).c_str());
            }
            std::cout << cprinter.ToString();
        } else if (FLAGS_ins_cmd == "put") {
            std::string key = FLAGS_ins_key;
            std::string value = FLAGS_ins_value;
            LOG(DEBUG, "key: %s, value: %s", key.c_str(), value.c_str());
            if (sdk.Put(key, value, &ins_err)) {
                LOG(DEBUG, "put success");
                if (ins_err == kUnknownUser) {
                    fprintf(stderr, "previous login may expired, please logout\n");
                }
            } else {
                LOG(FATAL, "put failed");
            }
        } else if (FLAGS_ins_cmd == "delete") {
            std::string key = FLAGS_ins_key;
            LOG(DEBUG, "key: %s", key.c_str());
            if (sdk.Delete(key, &ins_err)) {
                LOG(DEBUG, "delete success");
                if (ins_err == kUnknownUser) {
                    fprintf(stderr, "previous login may expired, please logout\n");
                }
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
                    std::cout << "value:" << value << std::endl;
                } else if (ins_err == kUnknownUser) {
                    fprintf(stderr, "previous login may expired, please logout\n");
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
            if (result->Error() == kUnknownUser) {
                fprintf(stderr, "previous login may expired, please logout\n");
            } else {
                int i = 0;
                while (!result->Done()) {
                    std::cout << "[" << ++i << "]\t" << result->Key() << " -> " << result->Value() << std::endl;
                    result->Next();
                }
            }
            delete result;
        } else if (FLAGS_ins_cmd == "watch") {
            std::string key = FLAGS_ins_key;
            bool done = false;
            bool ret = sdk.Watch(key, my_watch_callback, &done, &ins_err);
            if (!ret) {
                fprintf(stderr, "rpc error: %d\n", static_cast<int>(ins_err));
                return 1;
            }
            sdk.RegisterSessionTimeout(session_timeout_callback, NULL);
            while (!done) {
                sleep(1);
            }
            fprintf(stderr, "done\n");
        } else if (FLAGS_ins_cmd == "lock") {
            std::string key = FLAGS_ins_key;
            bool ret = sdk.Lock(key, &ins_err);
            if (!ret) {
                if (ins_err == kUnknownUser) {
                    fprintf(stderr, "previous login may expired, please logout\n");
                } else {
                    fprintf(stderr, "lock error: %d\n", static_cast<int>(ins_err));
                    return 1;
                }
            } else {
                sdk.RegisterSessionTimeout(session_timeout_callback, NULL);
                fprintf(stderr, "lock successful on %s\n", key.c_str());
                fprintf(stderr, "Press any key to release the lock.\n");
                getchar();
                ret = sdk.UnLock(key, &ins_err);
                if (ret) {
                    fprintf(stderr, "unlock successful on %s\n", key.c_str());
                }
            }
        } else if (FLAGS_ins_cmd == "clean") {
            std::string server_id = FLAGS_ins_rm_binlog_server_id;
            int64_t end_index = FLAGS_ins_rm_binlog_index;
            bool ret = sdk.CleanBinlog(server_id, end_index, &ins_err);
            if (!ret) {
                fprintf(stderr, "clean fail: %d\n", static_cast<int>(ins_err));
                return 1;
            }
            fprintf(stderr, "clean ok\n");
        } else if (FLAGS_ins_cmd == "stat") {
            TPrinter sprinter(11);
            sprinter.AddRow(11, "server id", "status", "kind", "Put", "Get",
                    "Delete", "Scan", "KeepAlive", "Lock", "Unlock", "Watch");
            std::vector<NodeStatInfo> stat_info;
            bool ret = sdk.ShowStatistics(&stat_info);
            if (!ret) {
                fprintf(stderr, "show statistics fail due to ins cluster issue\n");
                return 1;
            }
            for (std::vector<NodeStatInfo>::iterator it = stat_info.begin();
                    it != stat_info.end(); ++it) {
                const std::string& s_status = InsSDK::StatusToString(it->status);
                if (s_status != "Leader") {
                    continue;
                }
                sprinter.AddRow(11, it->server_id.c_str(), s_status.c_str(), "current",
                        boost::lexical_cast<std::string>(it->stats[0].current).c_str(),
                        boost::lexical_cast<std::string>(it->stats[1].current).c_str(),
                        boost::lexical_cast<std::string>(it->stats[2].current).c_str(),
                        boost::lexical_cast<std::string>(it->stats[3].current).c_str(),
                        boost::lexical_cast<std::string>(it->stats[4].current).c_str(),
                        boost::lexical_cast<std::string>(it->stats[5].current).c_str(),
                        boost::lexical_cast<std::string>(it->stats[6].current).c_str(),
                        boost::lexical_cast<std::string>(it->stats[7].current).c_str());
                sprinter.AddRow(11, "", "", "average",
                        boost::lexical_cast<std::string>(it->stats[0].average).c_str(),
                        boost::lexical_cast<std::string>(it->stats[1].average).c_str(),
                        boost::lexical_cast<std::string>(it->stats[2].average).c_str(),
                        boost::lexical_cast<std::string>(it->stats[3].average).c_str(),
                        boost::lexical_cast<std::string>(it->stats[4].average).c_str(),
                        boost::lexical_cast<std::string>(it->stats[5].average).c_str(),
                        boost::lexical_cast<std::string>(it->stats[6].average).c_str(),
                        boost::lexical_cast<std::string>(it->stats[7].average).c_str());
            }
            if (sprinter.Rows() == 0) {
                fprintf(stderr, "show statistics fail due to ins cluster issue\n");
                return 1;
            }
            std::cout << sprinter.ToString();
        } else if (FLAGS_ins_cmd == "login") {
            std::string username = FLAGS_ins_key;
            std::string password = FLAGS_ins_value;
            LOG(DEBUG, "user %s login", username.c_str());
            if (sdk.Login(username, password, &ins_err)) {
                LOG(DEBUG, "login finished");
                printf("login success\nperform actions for user %s now\n",
                       username.c_str());
                is_logged = true;
            } else {
                LOG(FATAL, "login failed");
                switch(ins_err) {
                case kUnknownUser: printf("user doesn't exist\n"); break;
                case kUserExists: printf("you have logged in\n"); break;
                case kPasswordError: printf("wrong password\n"); break;
                default: break;
                }
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
                printf("register success\n");
            } else {
                LOG(FATAL, "register failed");
                if (ins_err == kUserExists) {
                    printf("username has been registered\n");
                } else if (ins_err == kPasswordError) {
                    printf("password is not supposed to be empty.\n");
                }
            }
        } else if (FLAGS_ins_cmd == "whoami") {
            const std::string& uuid = sdk.GetCurrentUserID();
            if (uuid.empty()) {
                printf("You haven't logged in.\n");
            } else {
                printf("Your current uuid is %s.\n", uuid.c_str());
            }
        } else if (is_logged) {
            printf("not a valid command for logged user, "
                    "you may want to logout by `logout' command\n");
        } else {
            // Just exit
        }
        if (is_logged) {
            reset_flags();
        }
    } while(is_logged);

    return 0;
}
