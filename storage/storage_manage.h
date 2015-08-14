#ifndef _GALAXY_SDK_STORAGE_MANAGE_H_
#define _GALAXY_SDK_STORAGE_MANAGE_H_

#include <string>
#include <map>
#include <boost/function.hpp>
#include "common/mutex.h"
#include "leveldb/db.h"
#include "proto/ins_node.pb.h"

namespace galaxy {
namespace ins {

struct UserEntry {
    std::string username;
    leveldb::DB* db;
    UserEntry() : username(), db(NULL) { }
};

class StorageManager {
public:
    StorageManager(const std::string& data_dir);
    ~StorageManager();

    bool OpenDatabase(const std::string& uuid);
    void CloseDatabase(const std::string& uuid);

    DBStatus Get(const std::string& uuid, const std::string& key, std::string* value);
    DBStatus Put(const std::string& uuid, const std::string& key, const std::string& value);
private:
    std::string data_dir_;
    std::map<std::string, std::pair<Mutex*, leveldb::DB*> > dbs_;
};

}
}

#endif

