#ifndef _GALAXY_INS_USER_MANAGE_H_
#define _GALAXY_INS_USER_MANAGE_H_

#include <string>
#include <map>
#include "common/mutex.h"
#include "proto/ins_node.pb.h"

namespace galaxy {
namespace ins {

class UserManager {
public:
    UserManager() { }
    virtual ~UserManager() { }

    Status Login(const std::string& name, const std::string& password, std::string* uuid);
    Status Logout(const std::string& uuid);
    Status Register(const std::string& name, const std::string& password);
    Status DeleteUser(const std::string& name);

    bool IsLoggedIn(const std::string& name);
    bool IsValidUser(const std::string& name);

    void TruncateOnlineUsers();
    void TruncateAllUsers();

    static std::string CalcUuid(const std::string& name);
    static std::string CalcName(const std::string& uuid);
private:
    Mutex mu_;
    std::map<std::string, std::string> logged_users_;
    std::map<std::string, UserInfo> user_list_;
};

}
}

#endif

