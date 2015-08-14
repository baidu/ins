#include "storage_manage.h"

#include <assert.h>
#include "common/logging.h"
#include "leveldb/db.h"
#include "utils.h"

namespace galaxy {
namespace ins {

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
    dbs_[""].first = new Mutex();
    dbs_[""].second = default_db;
}

StorageManager::~StorageManager() {
    for (std::map<std::string, std::pair<Mutex*, leveldb::DB*> >::iterator it = dbs_.begin();
         it != dbs_.end(); ++it) {
        delete it->second.first;
        it->second.first = NULL;
        delete it->second.second;
        it->second.second = NULL;
    }
}

bool StorageManager::OpenDatabase(const std::string& uuid) {
    if (dbs_.find(uuid) != dbs_.end()) {
        LOG(INFO, "Log in a logged account :%s", uuid.c_str());
        return true;
    }
    dbs_[uuid].first = new Mutex();
    std::string full_name = data_dir_ + "/" + uuid + "@db";
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, full_name, &(dbs_[uuid].second));
    return status.ok();
}

void StorageManager::CloseDatabase(const std::string& uuid) {
    if (dbs_.find(uuid) == dbs_.end()) {
        LOG(WARNING, "Try to close an inexist database :%s", uuid.c_str());
        return;
    }
    {
    MutexLock lock(dbs_[uuid].first);
    delete dbs_[uuid].second;
    dbs_[uuid].second = NULL;
    }
    delete dbs_[uuid].first;
    dbs_[uuid].first = NULL;
}

DBStatus StorageManager::Get(const std::string& uuid,
                             const std::string& key,
                             std::string* value) {
    if (dbs_.find(uuid) == dbs_.end()) {
        LOG(WARNING, "Inexist or unlogged user :%s", uuid.c_str());
        return kNotFound;
    }
	if (value == NULL) {
		return kError;
	}
    MutexLock lock(dbs_[uuid].first);
    leveldb::Status status = dbs_[uuid].second->Get(leveldb::ReadOptions(), key, value);
	if (status.ok()) {
		return (value->empty()) ? kNotFound : kOk;
	}
	return kError;
}

DBStatus StorageManager::Put(const std::string& uuid,
                             const std::string& key,
                             const std::string& value) {
    if (dbs_.find(uuid) == dbs_.end()) {
        LOG(WARNING, "Inexist or unlogged user :%s", uuid.c_str());
        return kNotFound;
    }
    MutexLock lock(dbs_[uuid].first);
    leveldb::Status status = dbs_[uuid].second->Put(leveldb::WriteOptions(), key, value);
    return (status.ok()) ? kOk : kError;
}

}
}
