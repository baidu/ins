#ifndef _GALAXY_INS_USER_MANAGE_H_
#define _GALAXY_INS_USER_MANAGE_H_

#include <string>
#include <map>
#include "common/mutex.h"
#include "proto/ins_node.pb.h"
#include "storage/meta.h"

namespace galaxy {
namespace ins {

class UserManager {
public:
    UserManager() { }
    // User manager will automatically name the root user `root'
    UserManager(const UserInfo& root);
    virtual ~UserManager() { }

    Status Login(const std::string& name, const std::string& password, std::string* uuid);
    Status Logout(const std::string& uuid);
    Status Register(const std::string& name, const std::string& password);
    Status ForceOffline(const std::string& myid, const std::string& uuid);
    Status DeleteUser(const std::string& myid, const std::string& name);

    bool IsLoggedIn(const std::string& name);
    bool IsValidUser(const std::string& name);

    Status TruncateOnlineUsers(const std::string& myid);
    Status TruncateAllUsers(const std::string& myid);

    // Use it after valid uuid assured
    std::string GetUsernameFromUuid(const std::string& uuid) {
        return logged_users_[uuid];
    }

    static std::string CalcUuid(const std::string& name);
    static std::string CalcName(const std::string& uuid);
private:
    // Friend for accessing data more elegant
    friend void Meta::ReadUserList(UserManager* manager);
    friend void Meta::WriteUserList(const UserInfo& user);
private:
    Mutex mu_;
    std::map<std::string, std::string> logged_users_;
    std::map<std::string, UserInfo> user_list_;
};

}
}

#endif

