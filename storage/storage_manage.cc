#include "storage_manage.h"

#include <assert.h>
#include <gflags/gflags.h>
#include "common/logging.h"
#include "leveldb/db.h"
#include "utils.h"

DECLARE_bool(ins_data_compress);
DECLARE_int32(ins_data_block_size);
DECLARE_int32(ins_data_write_buffer_size);

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
    if (FLAGS_ins_data_compress) {
        options.compression = leveldb::kSnappyCompression;
        LOG(INFO, "enable snappy compress for data storage");
    }
    options.write_buffer_size = FLAGS_ins_data_write_buffer_size * 1024 * 1024;
    options.block_size = FLAGS_ins_data_block_size * 1024;
    LOG(INFO, "[data]: block_size: %d, writer_buffer_size: %d", 
        options.block_size,
        options.write_buffer_size);
    leveldb::DB* default_db = NULL;
    leveldb::Status status = leveldb::DB::Open(options, full_name, &default_db);
    assert(status.ok());
    dbs_[""] = default_db;
}

StorageManager::~StorageManager() {
    MutexLock lock(&mu_);
    for (std::map<std::string, leveldb::DB*>::iterator it = dbs_.begin();
         it != dbs_.end(); ++it) {
        delete it->second;
        it->second = NULL;
    }
}

bool StorageManager::OpenDatabase(const std::string& name) {
    {
        MutexLock lock(&mu_);
        if (dbs_.find(name) != dbs_.end()) {
            return true;
        }
    }
    std::string full_name = data_dir_ + "/" + name + "@db";
    leveldb::Options options;
    options.create_if_missing = true;
    if (FLAGS_ins_data_compress) {
        options.compression = leveldb::kSnappyCompression;
        LOG(INFO, "enable snappy compress for data storage");
    }
    options.write_buffer_size = FLAGS_ins_data_write_buffer_size * 1024 * 1024;
    options.block_size = FLAGS_ins_data_block_size * 1024;
    LOG(INFO, "[data]: block_size: %d, writer_buffer_size: %d", 
        options.block_size,
        options.write_buffer_size);
    leveldb::DB* current_db = NULL;
    leveldb::Status status = leveldb::DB::Open(options, full_name, &current_db);
    {
        MutexLock lock(&mu_);
        dbs_[name] = current_db;
    }
    return status.ok();
}

void StorageManager::CloseDatabase(const std::string& name) {
    MutexLock lock(&mu_);
    std::map<std::string, leveldb::DB*>::iterator dbs_it = dbs_.find(name);
    if (dbs_it == dbs_.end()) {
        return;
    }
    delete dbs_it->second;
    dbs_it->second = NULL;
    dbs_.erase(dbs_it);
}

Status StorageManager::Get(const std::string& name,
                           const std::string& key,
                           std::string* value) {
    if (value == NULL) {
        return kError;
    }
    leveldb::DB* db_ptr = NULL;
    {
        MutexLock lock(&mu_);
        if (dbs_.find(name) == dbs_.end()) {
            LOG(WARNING, "Inexist or unlogged user :%s", name.c_str());
            return kUnknownUser;
        }
        db_ptr = dbs_[name];
        if (db_ptr == NULL) {
            LOG(WARNING, "Try to access a closing database :%s", name.c_str());
            return kError;
        }
    }
    leveldb::Status status = db_ptr->Get(leveldb::ReadOptions(), key, value);
    return (status.ok()) ? kOk : ((status.IsNotFound()) ? kNotFound : kError);
}

Status StorageManager::Put(const std::string& name,
                           const std::string& key,
                           const std::string& value) {
    leveldb::DB* db_ptr = NULL;
    {
        MutexLock lock(&mu_);
        if (dbs_.find(name) == dbs_.end()) {
            LOG(WARNING, "Put fail, Inexist or unlogged user :%s", name.c_str());
            return kUnknownUser;
        }
        db_ptr = dbs_[name];
        if (db_ptr == NULL) {
            LOG(WARNING, "Try to access a closing database :%s", name.c_str());
            return kError;
        }
    }
    leveldb::Status status = db_ptr->Put(leveldb::WriteOptions(), key, value);
    return (status.ok()) ? kOk : kError;
}

Status StorageManager::Delete(const std::string& name,
                              const std::string& key) {
    leveldb::DB* db_ptr = NULL;
    {
        MutexLock lock(&mu_);
        if (dbs_.find(name) == dbs_.end()) {
            LOG(WARNING, "Inexist or unlogged user :%s", name.c_str());
            return kUnknownUser;
        }
        db_ptr = dbs_[name];
        if (db_ptr == NULL) {
            LOG(WARNING, "Try to access a closing database :%s", name.c_str());
            return kError;
        }
    }
    leveldb::Status status = db_ptr->Delete(leveldb::WriteOptions(), key);
    // Note: leveldb returns kOk even if the key is inexist
    return (status.ok()) ? kOk : kError;
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
    leveldb::DB* db_ptr = NULL;
    {
        MutexLock lock(&mu_);
        std::map<std::string, leveldb::DB*>::iterator dbs_it = dbs_.find(name);
        if (dbs_it == dbs_.end()) {
            LOG(WARNING, "Inexist or unlogged user :%s", name.c_str());
            return NULL;
        }
        db_ptr = dbs_it->second;
        if (db_ptr == NULL) {
            LOG(WARNING, "Try to access a closing database :%s", name.c_str());
            return NULL;
        }
    }
    return new StorageManager::Iterator(db_ptr, leveldb::ReadOptions());
}

}
}
