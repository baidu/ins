#ifndef _GALAXY_INS_USER_MANAGE_H_
#define _GALAXY_INS_USER_MANAGE_H_

#include <string>
#include <map>
#include "common/mutex.h"
#include "proto/ins_node.pb.h"
#include "storage/meta.h"
#include "leveldb/db.h"

namespace galaxy {
namespace ins {

class UserManager {
public:
    // User manager will automatically name the root user `root'
    UserManager(const std::string& data_dir, const UserInfo& root);
    virtual ~UserManager() { }

    Status Login(const std::string& name, const std::string& password, std::string* uuid);
    Status Logout(const std::string& uuid);
    Status Register(const std::string& name, const std::string& password);
    Status ForceOffline(const std::string& myid, const std::string& name);
    Status DeleteUser(const std::string& myid, const std::string& name);

    bool IsLoggedIn(const std::string& uuid);
    bool IsValidUser(const std::string& name);

    Status TruncateOnlineUsers(const std::string& myid);
    Status TruncateAllUsers(const std::string& myid);

    std::string GetUsernameFromUuid(const std::string& uuid);

    static std::string CalcUuid(const std::string& name);
    static std::string CalcName(const std::string& uuid);
private:
    bool WriteToDatabase(const UserInfo& user);
    bool WriteToDatabase(const std::string& name, const std::string& password);
    bool DeleteUserFromDatabase(const std::string& name);
    bool TruncateDatabase();
    bool RecoverFromDatabase();
private:
    Mutex mu_;
    std::map<std::string, std::string> logged_users_;
    std::map<std::string, UserInfo> user_list_;
    Mutex db_mu_;
    std::string data_dir_;
    leveldb::DB* user_db_;
};

}
}

#endif

