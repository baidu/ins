#include "storage_manage.h"

#include <assert.h>
#include "common/logging.h"
#include "leveldb/db.h"
#include "utils.h"

namespace galaxy {
namespace ins {

const std::string StorageManager::anonymous_user = "";

StorageManager::StorageManager(const std::string& data_dir) : data_dir_(data_dir) {
    bool ok = ins_common::Mkdirs(data_dir.c_str());
    if (!ok) {
        LOG(FATAL, "failed to create dir :%s", data_dir.c_str());
        abort();
    }
    // Create default database for shared namespace, i.e. anonymous user
    std::string full_name = data_dir + "/@db";
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::DB* default_db = NULL;
    leveldb::Status status = leveldb::DB::Open(options, full_name, &default_db);
    assert(status.ok());
    dbs_[""].db = default_db;
    dbs_[""].mu = new Mutex();
}

StorageManager::~StorageManager() {
    for (std::map<std::string, DBEntry>::iterator it = dbs_.begin();
         it != dbs_.end(); ++it) {
        delete it->second.db;
        it->second.db = NULL;
        delete it->second.mu;
        it->second.mu = NULL;
    }
}

bool StorageManager::OpenDatabase(const std::string& name) {
    if (dbs_.find(name) != dbs_.end()) {
        return true;
    }
    std::string full_name = data_dir_ + "/" + name + "@db";
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::DB* current_db = NULL;
    leveldb::Status status = leveldb::DB::Open(options, full_name, &current_db);
    dbs_[name].db = current_db;
    return status.ok();
}

void StorageManager::CloseDatabase(const std::string& name) {
    std::map<std::string, DBEntry>::iterator dbs_it = dbs_.find(name);
    if (dbs_it == dbs_.end()) {
        return;
    }
    MutexLock lock(dbs_it->second.mu);
    delete dbs_it->second.db;
    dbs_it->second.db = NULL;
    dbs_.erase(name);
}

Status StorageManager::Get(const std::string& name,
                           const std::string& key,
                           std::string* value) {
    if (value == NULL) {
        return kError;
    }
    if (dbs_.find(name) == dbs_.end()) {
        LOG(WARNING, "Inexist or unlogged user :%s", name.c_str());
        return kUnknownUser;
    }
    MutexLock lock(dbs_[name].mu);
    leveldb::Status status = dbs_[name].db->Get(leveldb::ReadOptions(), key, value);
    if (status.ok()) {
        return (value->empty()) ? kNotFound : kOk;
    }
    return kError;
}

Status StorageManager::Put(const std::string& name,
                           const std::string& key,
                           const std::string& value) {
    if (dbs_.find(name) == dbs_.end()) {
        LOG(WARNING, "Inexist or unlogged user :%s", name.c_str());
        return kUnknownUser;
    }
    MutexLock lock(dbs_[name].mu);
    leveldb::Status status = dbs_[name].db->Put(leveldb::WriteOptions(), key, value);
    return (status.ok()) ? kOk : kError;
}

Status StorageManager::Delete(const std::string& name,
                              const std::string& key) {
    if (dbs_.find(name) == dbs_.end()) {
        LOG(WARNING, "Inexist or unlogged user :%s", name.c_str());
        return kUnknownUser;
    }
    MutexLock lock(dbs_[name].mu);
    leveldb::Status status = dbs_[name].db->Delete(leveldb::WriteOptions(), key);
    return (status.ok()) ? kOk : ((status.IsNotFound()) ? kNotFound : kError);
}

std::string StorageManager::Iterator::key() const {
    return (it_ != NULL) ? it_->key().ToString() : "";
}

std::string StorageManager::Iterator::value() const {
    return (it_ != NULL) ? it_->value().ToString() : "";
}

StorageManager::Iterator *StorageManager::Iterator::Seek(std::string key) {
    if (it_ != NULL) {
        it_->Seek(key);
    }
    return this;
}

StorageManager::Iterator *StorageManager::Iterator::Next() {
    if (it_ != NULL) {
        it_->Next();
    }
    return this;
}

bool StorageManager::Iterator::Valid() const {
    return (it_ != NULL) ? it_->Valid() : false;
}

Status StorageManager::Iterator::status() const {
    return (it_ != NULL) ?
               ((it_->status().ok()) ?
                   kOk
                   :
                   ((it_->status().IsNotFound()) ?
                       kNotFound : kError
                   )
               )
               :
               kError
           ;
}

StorageManager::Iterator *StorageManager::NewIterator(const std::string& name) {
    std::map<std::string, DBEntry>::iterator dbs_it = dbs_.find(name);
    if (dbs_it == dbs_.end()) {
        LOG(WARNING, "Inexist or unlogged user :%s", name.c_str());
        return NULL;
    }
    return new StorageManager::Iterator(dbs_it->second.db, leveldb::ReadOptions());
}

}
}
