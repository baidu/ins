#include "ins_sdk.h"

#include <iostream>
#include <string>
#include <vector>
#include <gflags/gflags.h>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/scoped_ptr.hpp>
#include <stdlib.h>
#include <sys/stat.h>
#include "common/logging.h"
#include "common/tprinter.h"

DEFINE_bool(v, false, "verbose mode");
DEFINE_bool(i, false, "interactive mode");
DEFINE_bool(h, false, "help message");
DECLARE_string(cluster_members);
DECLARE_bool(help);
DECLARE_string(flagfile);

using namespace galaxy::ins::sdk;

static const int ERROR_OK = 0;
static const int ERROR_CLUSTER_DOWN = -1;
static const int ERROR_FALSE_RETVAL = 1;
static const int ERROR_SESSION_EXPIRED = 2;

void command_help_message() {
    std::cout << "command:" << std::endl
        << "  show                          show cluster" << std::endl
        << "  stat                          show statistics" << std::endl
        << "  put [key] [value]             update data" << std::endl
        << "  get [key]                     read data by key" << std::endl
        << "  delete [key]                  remove data by key" << std::endl
        << "  clean [start key] [end key]   remove data by key range" << std::endl
        << "  scan [start key] [end key]    scan data by key range" << std::endl
        << "  keys [start key] [end key]    scan keys by key range" << std::endl
        << "  watch [key]                   get event when value changed" << std::endl
        << "  lock [key]                    lock key" << std::endl
        << "  unlock [key]                  unlock key" << std::endl
        << "  register [user] [password]    register new user" << std::endl
        << "  login [user] [password]       start operating as a user" << std::endl
        << "  logout                        stop operating with user" << std::endl
        << "  whoami                        show current session and uuid" << std::endl
        << "  exit                          exit program" << std::endl
        << "  quit                          same as exit" << std::endl;
}

void help_message() {
    std::cout << "Usage: ncli [-i] [-v] COMMAND ARGS..." << std::endl
        << std::endl
        << "    -h       print this help message" << std::endl
        << "    -v       verbose mode" << std::endl
        << "    -i       interactive mode" << std::endl
        << std::endl;
    command_help_message();
}

void session_timeout_callback(void* /*context*/) {
    std::cout << "session timeout" << std::endl;
}

void my_watch_callback(const WatchParam& param, SDKError error) {
    if (error == kUnknownUser) {
        std::cerr << "previous login may expired, please logout" << std::endl;
    } else if (error == kClusterDown) {
        std::cerr << "watch rpc failed: " << InsSDK::ErrorToString(error) << std::endl;
    } else {
        std::cout << std::endl << "key: " << param.key << std::endl
            << "value: " << param.value << std::endl
            << "deleted: " << (param.deleted ? "true" : "false") << std::endl
            << "error code: " << InsSDK::ErrorToString(error) << std::endl;
    }
}

int show_status(InsSDK& sdk) {
    TPrinter cprinter(7);
    cprinter.AddRow(7, "server node", "role", "term", "last_log_index",
                   "last_log_term", "commit_index", "last_applied");
    std::vector<ClusterNodeInfo> cluster_info;
    sdk.ShowCluster(&cluster_info);
    for (std::vector<ClusterNodeInfo>::iterator it = cluster_info.begin();
            it != cluster_info.end(); ++it) {
        std::string s_status = InsSDK::StatusToString(it->status);
        cprinter.AddRow(7, it->server_id.c_str(), s_status.c_str(),
                boost::lexical_cast<std::string>(it->term).c_str(),
                boost::lexical_cast<std::string>(it->last_log_index).c_str(),
                boost::lexical_cast<std::string>(it->last_log_term).c_str(),
                boost::lexical_cast<std::string>(it->commit_index).c_str(),
                boost::lexical_cast<std::string>(it->last_applied).c_str());
    }
    std::cout << cprinter.ToString();
    return ERROR_OK;
}

int show_statistics(InsSDK& sdk) {
    TPrinter sprinter(11);
    sprinter.AddRow(11, "server id", "status", "kind", "Put", "Get",
            "Delete", "Scan", "KeepAlive", "Lock", "Unlock", "Watch");
    std::vector<NodeStatInfo> stat_info;
    bool ret = sdk.ShowStatistics(&stat_info);
    if (!ret) {
        std::cerr << "show statistics fail due to cluster issue" << std::endl;
        return ERROR_CLUSTER_DOWN;
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
        std::cerr << "show statistics fail due to cluster issue" << std::endl;
        return ERROR_CLUSTER_DOWN;
    }
    std::cout << sprinter.ToString();
    return ERROR_OK;
}

int put_kv(InsSDK& sdk, const std::string& key, const std::string& value) {
    SDKError error;
    if (sdk.Put(key, value, &error)) {
        if (error == kUnknownUser) {
            std::cerr << "previous login may expired, please logout" << std::endl;
            return ERROR_SESSION_EXPIRED;
        }
    } else {
        std::cerr << "put failed: " << InsSDK::ErrorToString(error) << std::endl;
        return ERROR_CLUSTER_DOWN;
    }
    return ERROR_OK;
}

int get_kv(InsSDK& sdk, const std::string& key) {
    std::string value;
    SDKError error;
    if (sdk.Get(key, &value, &error)) {
        if (error == kOK) {
            std::cout << (FLAGS_i ? "value: " : "") << value << std::endl;
        } else if (error == kUnknownUser) {
            std::cerr << "previous login may expired, please logout" << std::endl;
            return ERROR_SESSION_EXPIRED;
        } else {
            std::cout << "NOT FOUND" << std::endl;
            return ERROR_FALSE_RETVAL;
        }
    } else {
        std::cerr << "get failed: " << InsSDK::ErrorToString(error) << std::endl;
        return ERROR_CLUSTER_DOWN;
    }
    return ERROR_OK;
}

int delete_kv(InsSDK& sdk, const std::string& key) {
    SDKError error;
    if (sdk.Delete(key, &error)) {
        if (error == kUnknownUser) {
            std::cerr << "previous login may expired, please logout" << std::endl;
            return ERROR_SESSION_EXPIRED;
        }
    } else {
        std::cerr << "delete failed: " << InsSDK::ErrorToString(error) << std::endl;
        return ERROR_CLUSTER_DOWN;
    }
    return ERROR_OK;
}

int delete_data(InsSDK& sdk, const std::string& start_key, const std::string& end_key) {
    ScanResult* result = sdk.Scan(start_key, end_key);
    boost::scoped_ptr<ScanResult> result_guard(result);
    if (result->Error() == kUnknownUser) {
        std::cerr << "previous login may expired, please logout" << std::endl;
        return ERROR_SESSION_EXPIRED;
    } else {
        while (!result->Done()) {
            SDKError error;
            sdk.Delete(result->Key(), &error);
            if (error != kOK) {
                std::cerr << "delete `" << result->Key() << "' failed: " << InsSDK::ErrorToString(error) << std::endl;
            }
            result->Next();
        }
        if (result->Error() != kOK) {
            std::cerr << "scan failed: " << InsSDK::ErrorToString(result->Error()) << std::endl;
            return ERROR_CLUSTER_DOWN;
        }
    }
    return ERROR_OK;
}

int scan_data(InsSDK& sdk, const std::string& start_key, const std::string& end_key, bool key_only = false) {
    ScanResult* result = sdk.Scan(start_key, end_key);
    boost::scoped_ptr<ScanResult> result_guard(result);
    if (result->Error() == kUnknownUser) {
        std::cerr << "previous login may expired, please logout" << std::endl;
        return ERROR_SESSION_EXPIRED;
    } else {
        int i = 0;
        while (!result->Done()) {
            if (FLAGS_i) {
                if (key_only) {
                    std::cout << "[" << ++i << "]\t`" << result->Key() << "'" << std::endl;
                } else {
                    std::cout << "[" << ++i << "]\t`" << result->Key() << "' -> `" << result->Value() << "'" << std::endl;
                }
            } else {
                if (key_only) {
                    std::cout << result->Key() << std::endl;
                } else {
                    std::cout << result->Key() << "\t" << result->Value() << std::endl;
                }
            }
            result->Next();
        }
        if (result->Error() != kOK) {
            std::cerr << "scan failed: " << InsSDK::ErrorToString(result->Error()) << std::endl;
            return ERROR_CLUSTER_DOWN;
        }
    }
    return ERROR_OK;
}

int list_data(InsSDK& sdk, std::string dir, bool key_only = false) {
    static std::string last_dir;
    static std::vector<KVPair> last_result;
    if (*dir.rbegin() != '/') {
        dir.push_back('/');
    }
    if (last_dir.empty() || std::mismatch(dir.begin(), dir.end(), last_dir.begin()).first != dir.begin()) {
        ScanResult* result = sdk.Scan(dir, dir + '\xff');
        boost::scoped_ptr<ScanResult> result_guard(result);
        last_result.clear();
        if (result->Error() == kUnknownUser) {
            std::cerr << "previous login may expired, please logout" << std::endl;
            last_dir = "";
            return ERROR_SESSION_EXPIRED;
        } else {
            while (!result->Done()) {
                KVPair pair;
                pair.key = result->Key();
                pair.value = result->Value();
                last_result.push_back(pair);
                result->Next();
            }
            if (result->Error() != kOK) {
                std::cerr << "scan failed: " << InsSDK::ErrorToString(result->Error()) << std::endl;
                last_dir = "";
                return ERROR_CLUSTER_DOWN;
            }
        }
    }
    int i = 0;
    std::vector<KVPair>::iterator it = last_result.begin();
    for (; it != last_result.end(); ++it) {
        if (std::mismatch(dir.begin(), dir.end(), it->key.begin()).first == dir.end()) {
            break;
        }
    }
    std::string last_prefix;
    dir.erase(dir.end() - 1);
    for (; it != last_result.end(); ++it) {
        std::pair<std::string::iterator, std::string::iterator> result =
                std::mismatch(dir.begin(), dir.end(), it->key.begin());
        if (result.first != dir.end()) {
            break;
        }
        size_t cur_level_mark = it->key.find('/', result.second - it->key.begin() + 1);
        std::string cur_key = it->key.substr(0, cur_level_mark);
        if (cur_key == last_prefix) {
            continue;
        }
        last_prefix = cur_key;
        std::string cur_value = it->value;
        if (FLAGS_i) {
            cur_value = std::string("`") + it->value + std::string("'");
        }
        if (cur_key != it->key) {
            cur_value = "NOT FOUND";
        }
        if (FLAGS_i) {
            if (key_only) {
                std::cout << "[" << ++i << "]\t" << cur_key << std::endl;
            } else {
                std::cout << "[" << ++i << "]\t" << cur_key << " -> " << cur_value << std::endl;
            }
        } else {
            if (key_only) {
                std::cout << cur_key << std::endl;
            } else {
                std::cout << cur_key << "\t" << cur_value << std::endl;
            }
        }
    }
    last_dir = dir;
    return ERROR_OK;
}

int watch_kv(InsSDK& sdk, const std::string& key) {
    SDKError error;
    bool ret = sdk.Watch(key, my_watch_callback, NULL, &error);
    if (!ret) {
        std::cerr << "watch failed: " << InsSDK::ErrorToString(error) << std::endl;
        return ERROR_CLUSTER_DOWN;
    }
    sdk.RegisterSessionTimeout(session_timeout_callback, NULL);
    return ERROR_OK;
}

int lock_kv(InsSDK& sdk, const std::string& key) {
    SDKError error;
    bool ret = sdk.Lock(key, &error);
    if (!ret) {
        if (error == kUnknownUser) {
            std::cerr << "previous login may expired, please logout" << std::endl;
            return ERROR_SESSION_EXPIRED;
        } else {
            std::cerr << "lock failed: " << InsSDK::ErrorToString(error) << std::endl;
            return ERROR_CLUSTER_DOWN;
        }
    } else {
        sdk.RegisterSessionTimeout(session_timeout_callback, NULL);
    }
    return ERROR_OK;
}

int unlock_kv(InsSDK& sdk, const std::string& key) {
    SDKError error;
    bool ret = sdk.UnLock(key, &error);
    if (!ret) {
        if (error == kUnknownUser) {
            std::cerr << "previous login may expired, please logout" << std::endl;
            return ERROR_SESSION_EXPIRED;
        } else {
            std::cerr << "unlock failed: " << InsSDK::ErrorToString(error) << std::endl;
            return ERROR_CLUSTER_DOWN;
        }
    }
    return ERROR_OK;
}

int whoami(InsSDK& sdk) {
    const std::string& sessionid = sdk.GetSessionID();
    std::cout << "session id: " << sessionid << std::endl;
    if (sdk.IsLoggedIn()) {
        const std::string& uuid = sdk.GetCurrentUserID();
        std::cout << "login uuid: " << uuid << std::endl;
    } else {
        std::cout << "not login" << std::endl;
    }
    return ERROR_OK;
}

int register_user(InsSDK& sdk, const std::string& username, const std::string& password) {
    SDKError error;
    if (!sdk.Register(username, password, &error)) {
        if (error == kUserExists) {
            std::cerr << "user has been registered" << std::endl;
            return ERROR_FALSE_RETVAL;
        } else if (error == kPasswordError) {
            std::cerr << "empty password" << std::endl;
            return ERROR_FALSE_RETVAL;
        } else {
            std::cerr << "register failed: " << InsSDK::ErrorToString(error) << std::endl;
            return ERROR_CLUSTER_DOWN;
        }
    }
    return ERROR_OK;
}

int login_user(InsSDK& sdk, const std::string& username, const std::string& password) {
    SDKError error;
    if (!sdk.Login(username, password, &error)) {
        switch (error) {
        case kUnknownUser:
            std::cerr << "unknown user: " << username << std::endl;
            return ERROR_FALSE_RETVAL;
        case kUserExists:
            std::cerr << "you have logged in as " << sdk.GetCurrentUserID() << std::endl;
            return ERROR_FALSE_RETVAL;
        case kPasswordError:
            std::cerr << "wrong password" << std::endl;
            return ERROR_FALSE_RETVAL;
        default:
            std::cerr << "login failed: " << InsSDK::ErrorToString(error) << std::endl;
            return ERROR_CLUSTER_DOWN;
        }
    }
    return ERROR_OK;
}

int logout_user(InsSDK& sdk) {
    SDKError error;
    if (!sdk.Logout(&error)) {
        std::cerr << "logout failed: " << InsSDK::ErrorToString(error) << std::endl;
        return ERROR_CLUSTER_DOWN;
    }
    if (error == kUnknownUser) {
        std::cout << "user has logged out" << std::endl;
    }
    return ERROR_OK;
}

inline bool get_user_cmd(std::string& cmd) {
    std::cout << "nexus > ";
    std::cout.flush();
    std::getline(std::cin, cmd);
    return std::cin.good();
}

inline void fill_string(std::string& str, std::istream& preferred, std::istream& backup,
        const std::string& notifier) {
    preferred >> str;
    if (str.empty() && FLAGS_i) {
        if (!notifier.empty()) {
            std::cout << notifier;
            std::cout.flush();
        }
        std::getline(backup, str);
    }
}

inline void fill_kv(std::string& key, std::string& value, std::istream& preferred, std::istream& backup) {
    preferred >> key;
    if (key.empty() && FLAGS_i) {
        std::cout << "  key > ";
        std::cout.flush();
        std::getline(backup, key);
        std::cout << "  value > ";
        std::cout.flush();
        std::getline(backup, value);
    } else {
        preferred >> std::ws;
        std::getline(preferred, value);
    }
}

inline bool file_exists(const char* filename) {
    struct stat buffer;
    return stat(filename, &buffer) == 0;
}

int main(int argc, char* argv[]) {
    if (file_exists("./nexus.flag")) {
        FLAGS_flagfile = "./nexus.flag";
    } else if (file_exists("../conf/nexus.flag")) {
        FLAGS_flagfile = "../conf/nexus.flag";
    }
    std::vector<std::string> members;
    google::ParseCommandLineNonHelpFlags(&argc, &argv, true);
    boost::split(members, FLAGS_cluster_members,
            boost::is_any_of(","), boost::token_compress_on);
    if (members.size() < 1) {
        std::cerr << "no valid member in configuration" << std::endl;
        return ERROR_CLUSTER_DOWN;
    }
    if (FLAGS_h || FLAGS_help) {
        help_message();
        return ERROR_FALSE_RETVAL;
    }
    if (FLAGS_v) {
        ins_common::SetLogLevel(ins_common::DEBUG);
    } else {
        ins_common::SetLogLevel(ins_common::FATAL);
    }
    InsSDK sdk(members);
    std::string command;
    if (FLAGS_i) {
        get_user_cmd(command);
    } else {
        if (argc < 2) {
            help_message();
            return ERROR_FALSE_RETVAL;
        }
        std::stringstream ss;
        for (int i = 1; i < argc; ++i) {
            ss << argv[i] << " ";
        }
        command = ss.str();
        command.erase(command.end() - 1);
    }
    int retval = ERROR_OK;
    if (command == "exit" || command == "quit") {
        return retval;
    }
    do {
        std::stringstream ss(command);
        std::string operation;
        ss >> operation;
        if (operation == "show") {
            retval = show_status(sdk);
        } else if (operation == "put") {
            std::string key, value;
            fill_kv(key, value, ss, std::cin);
            retval = put_kv(sdk, key, value);
        } else if (operation == "get") {
            std::string key;
            fill_string(key, ss, std::cin, "  key > ");
            retval = get_kv(sdk, key);
        } else if (operation == "delete") {
            std::string key;
            fill_string(key, ss, std::cin, "  key > ");
            retval = delete_kv(sdk, key);
        } else if (operation == "clean") {
            std::string start_key, end_key;
            fill_string(start_key, ss, std::cin, "  start key > ");
            fill_string(end_key, ss, std::cin, "  end key > ");
            retval = delete_data(sdk, start_key, end_key);
        } else if (operation == "scan") {
            std::string start_key, end_key;
            fill_string(start_key, ss, std::cin, "  start key > ");
            fill_string(end_key, ss, std::cin, "  end key > ");
            retval = scan_data(sdk, start_key, end_key, false);
        } else if (operation == "keys") {
            std::string start_key, end_key;
            fill_string(start_key, ss, std::cin, "  start key > ");
            fill_string(end_key, ss, std::cin, "  end key > ");
            retval = scan_data(sdk, start_key, end_key, true);
        } else if (operation == "ls") {
            std::string dir;
            fill_string(dir, ss, std::cin, "  dir > ");
            retval = list_data(sdk, dir, false);
        } else if (operation == "keyls") {
            std::string dir;
            fill_string(dir, ss, std::cin, "  dir > ");
            retval = list_data(sdk, dir, true);
        } else if (operation == "watch") {
            std::string key;
            fill_string(key, ss, std::cin, "  key > ");
            retval = watch_kv(sdk, key);
        } else if (operation == "lock") {
            std::string key;
            fill_string(key, ss, std::cin, "  key > ");
            retval = lock_kv(sdk, key);
            if (retval == ERROR_OK && !FLAGS_i) {
                std::cout << "press enter to release the lock > ";
                std::cout.flush();
                std::getline(std::cin, command);
                retval = unlock_kv(sdk, key);
            }
        } else if (operation == "unlock") {
            if (!FLAGS_i) {
                std::cerr << "unlock function is available in interactive mode" << std::endl;
                retval = ERROR_FALSE_RETVAL;
            } else {
                std::string key;
                fill_string(key, ss, std::cin, "  key > ");
                retval = unlock_kv(sdk, key);
            }
        } else if (operation == "stat") {
            retval = show_statistics(sdk);
        } else if (operation == "login") {
            if (!FLAGS_i) {
                std::cerr << "login function is available in interactive mode" << std::endl;
                retval = ERROR_FALSE_RETVAL;
            } else {
                std::string username, password;
                fill_string(username, ss, std::cin, "  username > ");
                fill_string(password, ss, std::cin, "  password > ");
                retval = login_user(sdk, username, password);
            }
        } else if (operation == "logout") {
            if (!FLAGS_i) {
                std::cerr << "logout function is available in interactive mode" << std::endl;
                retval = ERROR_FALSE_RETVAL;
            } else {
                retval = logout_user(sdk);
            }
        } else if (operation == "register") {
            std::string username, password;
            fill_string(username, ss, std::cin, "  username > ");
            fill_string(password, ss, std::cin, "  password > ");
            retval = register_user(sdk, username, password);
        } else if (operation == "whoami") {
            retval = whoami(sdk);
        } else if (operation == "help") {
            if (FLAGS_i) {
                command_help_message();
            } else {
                help_message();
            }
            retval = ERROR_OK;
        } else if (!operation.empty()) {
            std::cerr << "command is not valid" << std::endl;
            retval = ERROR_FALSE_RETVAL;
        }
    } while (FLAGS_i && get_user_cmd(command) && command != "quit" && command != "exit");

    if (FLAGS_i) {
        return ERROR_OK;
    }

    return retval;
}

