#include "server/user_manage.h"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/lexical_cast.hpp>
#include <string>
#include <sstream>
#include "common/logging.h"
#include "common/timer.h"
#include "leveldb/write_batch.h"
#include "storage/utils.h"

namespace galaxy {
namespace ins {

const std::string user_dbname = "userdb";
const std::string root_name = "root";

std::string UserManager::CalcUuid(const std::string& name) {
    boost::uuids::uuid uuid_namespace = boost::uuids::random_generator()();
    boost::uuids::uuid uuid = boost::uuids::name_generator(uuid_namespace)(name);
    std::stringstream uuids;
    uuids << uuid;
    return uuids.str();
}

UserManager::UserManager(const std::string& data_dir,
                         const UserInfo& root) : data_dir_(data_dir) {
    bool ok = ins_common::Mkdirs(data_dir.c_str());
    if (!ok) {
        LOG(FATAL, "failed to create dir :%s", data_dir.c_str());
        abort();
    }
    std::string full_name = data_dir + "/" + user_dbname;
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, full_name, &user_db_);
    assert(status.ok());
    if (root.has_username() && root.has_passwd()) {
        assert(WriteToDatabase(root));
    }
    RecoverFromDatabase();
}

Status UserManager::Login(const std::string& name,
                          const std::string& password,
                          const std::string& uuid) {
    MutexLock lock(&mu_);
    std::map<std::string, UserInfo>::iterator user_it = user_list_.find(name);
    if (user_it == user_list_.end()) {
        LOG(WARNING, "Inexist user tried to login :%s", name.c_str());
        return kUnknownUser;
    }
    if (user_it->second.passwd() != password) {
        LOG(WARNING, "Password error for logging :%s", name.c_str());
        return kPasswordError;
    }
    if (uuid.empty()) {
        LOG(FATAL, "provide improper uuid :%s", name.c_str());
        return kError;
    }

    logged_users_[uuid] = name;
    return kOk;
}

Status UserManager::Logout(const std::string& uuid) {
    MutexLock lock(&mu_);
    std::map<std::string, std::string>::iterator online_it = logged_users_.find(uuid);
    if (online_it == logged_users_.end()) {
        LOG(WARNING, "Logout for an inexist user :%s", uuid.c_str());
        return kUnknownUser;
    }

    logged_users_.erase(online_it);
    return kOk;
}

Status UserManager::Register(const std::string& name, const std::string& password) {
    MutexLock lock(&mu_);
    if (name.empty()) {
        LOG(WARNING, "Cannot register a user without username");
        // Return `exist' status since empty user is consider as default in storage
        return kUserExists;
    }
    std::map<std::string, UserInfo>::iterator user_it = user_list_.find(name);
    if (user_it != user_list_.end()) {
        LOG(WARNING, "Try to register an exist user :%s", name.c_str());
        return kUserExists;
    }
    if (!WriteToDatabase(name, password)) {
        return kError;
    }
    user_list_[name].set_username(name);
    user_list_[name].set_passwd(password);
    LOG(INFO, "%s registered ok.", name.c_str());
    return kOk;
}

Status UserManager::ForceOffline(const std::string& myid, const std::string& name) {
    MutexLock lock(&mu_);
    std::map<std::string, std::string>::const_iterator online_it = logged_users_.find(myid);
    if (online_it == logged_users_.end()) {
        return kUnknownUser;
    }
    std::map<std::string, UserInfo>::const_iterator user_it = user_list_.find(name);
    if (user_it == user_list_.end()) {
        return kUnknownUser;
    }
    if (online_it->second != root_name && online_it->second != name) {
        return kPermissionDenied;
    }

    std::map<std::string, std::string>::iterator it = logged_users_.begin();
    while (it != logged_users_.end()) {
        if (it->second == name) {
            logged_users_.erase(it++);
        } else {
            ++it;
        }
    }
    return kOk;
}

Status UserManager::DeleteUser(const std::string& myid, const std::string& name) {
    MutexLock lock(&mu_);
    std::map<std::string, std::string>::const_iterator online_it = logged_users_.find(myid);
    if (online_it == logged_users_.end()) {
        return kUnknownUser;
    }
    if (online_it->second != root_name && online_it->second != name) {
        return kPermissionDenied;
    }
    std::map<std::string, UserInfo>::iterator user_it = user_list_.find(name);
    if (user_it == user_list_.end()) {
        LOG(WARNING, "Try to delete an inexist user :%s", name.c_str());
        return kNotFound;
    }
    if (!DeleteUserFromDatabase(name)) {
        return kError;
    }

    std::map<std::string, std::string>::iterator it = logged_users_.begin();
    while (it != logged_users_.end()) {
        if (it->second == name) {
            logged_users_.erase(it++);
        } else {
            ++it;
        }
    }
    user_list_.erase(user_it);
    return kOk;
}

bool UserManager::IsLoggedIn(const std::string& uuid) {
    MutexLock lock(&mu_);
    return logged_users_.find(uuid) != logged_users_.end();
}

bool UserManager::IsValidUser(const std::string& name) {
    MutexLock lock(&mu_);
    return user_list_.find(name) != user_list_.end();
}

Status UserManager::TruncateOnlineUsers(const std::string& myid) {
    MutexLock lock(&mu_);
    std::map<std::string, std::string>::const_iterator online_it = logged_users_.find(myid);
    if (online_it == logged_users_.end()) {
        return kUnknownUser;
    }
    if (online_it->second != root_name) {
        return kPermissionDenied;
    }
    std::map<std::string, std::string>::iterator it = logged_users_.begin();
    while (it != logged_users_.end()) {
        if (it->second != root_name) {
            logged_users_.erase(it++);
        } else {
            ++it;
        }
    }
    return kOk;
}

Status UserManager::TruncateAllUsers(const std::string& myid) {
    UserInfo root;
    root.set_username(root_name);
    {
        MutexLock lock(&mu_);
        std::map<std::string, std::string>::const_iterator online_it = logged_users_.find(myid);
        if (online_it == logged_users_.end()) {
            return kUnknownUser;
        }
        if (online_it->second != root_name) {
            return kPermissionDenied;
        }
        root.set_passwd(user_list_[root_name].passwd());
    }
    if (!TruncateDatabase()) {
        return kError;
    }
    if (!WriteToDatabase(root)) {
        return kError;
    }
    MutexLock lock(&mu_);
    std::map<std::string, std::string>::iterator it = logged_users_.begin();
    while (it != logged_users_.end()) {
        if (it->second != root_name) {
            logged_users_.erase(it++);
        } else {
            ++it;
        }
    }
    user_list_.clear();
    user_list_[root_name].set_username(root_name);
    user_list_[root_name].set_passwd(root.passwd());
    return kOk;
}

std::string UserManager::GetUsernameFromUuid(const std::string& uuid) {
    MutexLock lock(&mu_);
    if (logged_users_.find(uuid) != logged_users_.end()){
        return logged_users_[uuid];
    }
    return "";
}

bool UserManager::WriteToDatabase(const UserInfo& user) {
    if (!user.has_username() || !user.has_passwd()) {
        return false;
    }
    leveldb::Status status = user_db_->Put(leveldb::WriteOptions(),
                                           user.username(),
                                           user.passwd());
    return status.ok();
}

bool UserManager::WriteToDatabase(const std::string& name, const std::string& password) {
    leveldb::Status status = user_db_->Put(leveldb::WriteOptions(), name, password);
    return status.ok();
}

bool UserManager::DeleteUserFromDatabase(const std::string& name) {
    leveldb::Status status = user_db_->Delete(leveldb::WriteOptions(), name);
    return status.ok();
}

bool UserManager::TruncateDatabase() {
    leveldb::Iterator* it = user_db_->NewIterator(leveldb::ReadOptions());
    leveldb::WriteBatch batch;
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        batch.Delete(it->key().ToString());
    }
    if (!it->status().ok()) {
        return false;
    }
    leveldb::Status status = user_db_->Write(leveldb::WriteOptions(), &batch);
    return status.ok();
}

bool UserManager::RecoverFromDatabase() {
    leveldb::Iterator* it = user_db_->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        const std::string& name = it->key().ToString();
        user_list_[name].set_username(name);
        user_list_[name].set_passwd(it->value().ToString());
    }
    bool result = it->status().ok();
    delete it;
    return result;
}

}
}
