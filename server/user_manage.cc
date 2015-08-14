#include "user_manage.h"

#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transfrom_width.hpp>
#include <boost/archive/iterators/ostream_iterator.hpp>
#include <string>
#include <sstream>

static std::string UserManager::CalcUuid(const std::string& name) {
    using namespace boost::archive::iterators;
    std::stringstream uuid;
    typedef base64_from_binary <
        transform_width <
            const char *,
            6,
            8
        >
    > base64_text;

    std::copy(base64_text(name.c_str()),
              base64_text(name.c_str() + name.size()),
              std::ostream_iterator<char>(uuid));

    return uuid.str();
}

static std::string UserManager::CalcName(const std::string& uuid) {
    using namespace boost::archive::iterators;
    std::stringstream name;
    typedef transform_width <
        binary_from_base64 <
            const char *
        >,
        8,
        6
    > base64_code;

    std::copy(base64_code(uuid.c_str()),
              base64_code(uuid.c_str() + uuid.size()),
              std::ostream_iterator<char>(name));
    return name.str();
}

Status UserManager::Login(const std::string& name,
                          const std::string& password,
                          std::string* uuid) {
    MutexLock lock(&mu_);
    std::map<std::string, UserInfo>::iterator user_it = user_list_.find(name);
    if (user_it == user_list_.end()) {
        LOG(WARNING, "Inexist user tried to login :%s", name.c_str());
        return kNotFound;
    }
    // TODO Seems that undefined optional field has undefined behaviour while reading
    if (user_it->second.uuid() != "") {
        LOG(WARNING, "Try to log in a logged account :%s", name.c_str());
        return kError;
    }
    if (user_it->second.passwd() != password) {
        LOG(WARNING, "Password error for logging :%s", name.c_str());
        return kError;
    }

    user_it->second.set_uuid(CalcUuid(name));
    logged_users_[user_it->second.uuid()] = name;
    if (uuid != NULL) {
        *uuid = user_it->second.uuid();
    }
    return kOk;
}

Status UserManager::Logout(const std::string& uuid) {
    MutexLock lock(&mu_);
    std::map<std::string, UserInfo>::iterator user_it = user_list_.find(name);
    if (user_it == user_list_.end()) {
        LOG(WARNING, "Logout for an inexist user :%s", name.c_str());
        return kNotFound;
    }
    if (user_it->second.uuid() == "") {
        LOG(WARNING, "Try to log in a logged account :%s", name.c_str());
        return kError;
    }

    logged_user_.erase(uuid);
    user_it->second.set_uuid("");
    return kOk;
}

