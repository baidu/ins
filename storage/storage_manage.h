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

struct DBEntry {
    leveldb::DB* db;
    Mutex* mu;
    DBEntry() : db(NULL), mu(NULL) {
	}
};

class StorageManager {
public:
    StorageManager(const std::string& data_dir);
    ~StorageManager();

    bool OpenDatabase(const std::string& name);
    void CloseDatabase(const std::string& name);

    Status Get(const std::string& name, const std::string& key, std::string* value);
    Status Put(const std::string& name, const std::string& key, const std::string& value);
    Status Delete(const std::string& name, const std::string& key);

	static const std::string anonymous_user;
private:
    std::string data_dir_;
    std::map<std::string, DBEntry> dbs_;
};

}
}

#endif

