#include "binlog.h"

#include "common/asm_atomic.h"
#include "common/logging.h"
#include "leveldb/write_batch.h"

namespace galaxy {
namespace ins {

const std::string log_dbname = "binlog";
const std::string length_tag = "#BINLOG_LEN#";

BinLogger::BinLogger(const std::string& data_dir) : db_(NULL),
                                                    length_(0) {
    std::string full_name = data_dir + "/" + log_dbname;
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, full_name, &db_);
    assert(status.ok());
    std::string value;
    status = db_->Get(leveldb::ReadOptions(), length_tag, &value);
    if (status.ok() && !value.empty()) {
        length_ = StringToInt(value);
    }
}

BinLogger::~BinLogger() {
    delete db_;
}

int64_t BinLogger::GetLength() {
    MutexLock lock(&mu_);
    return length_;
}

std::string BinLogger::IntToString(int64_t num) {
    std::string key;
    key.resize(sizeof(int64_t));
    memcpy(&key[0], &num, sizeof(int64_t));
    return key;
}

int64_t BinLogger::StringToInt(const std::string& s) {
    assert(s.size() == sizeof(int64_t));
    int64_t num = 0;
    memcpy(&num, &s[0], sizeof(int64_t));
    return num;
}

bool BinLogger::ReadSlot(int64_t slot_index, LogEntry* log_entry) {
    std::string value;
    std::string key = IntToString(slot_index);
    leveldb::Status status = db_->Get(leveldb::ReadOptions(), key, &value);
    assert(status.ok());
    LoadLogEntry(value, log_entry);
    return true;
}

void BinLogger::AppendEntry(const LogEntry& log_entry) {
    std::string buf;
    DumpLogEntry(log_entry, &buf);
    std::string cur_index;
    std::string next_index;
    {
        MutexLock lock(&mu_);
        cur_index = IntToString(length_);
        length_++;
        next_index = IntToString(length_);
    }
    leveldb::WriteBatch batch;
    batch.Put(cur_index, buf);
    batch.Put(length_tag, next_index);
    leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);
    assert(status.ok());
}

void BinLogger::Truncate(int64_t trunk_slot_index) {
    if (trunk_slot_index < -1) {
        trunk_slot_index = -1;
    }

    {
        MutexLock lock(&mu_);
        length_ = trunk_slot_index + 1;
    }

    leveldb::Status status = db_->Put(leveldb::WriteOptions(), 
                                      length_tag, IntToString(length_));
    assert(status.ok());
}

void BinLogger::DumpLogEntry(const LogEntry& log_entry, std::string* buf) {
    assert(buf);
    int32_t total_len = sizeof(uint8_t) 
                        + sizeof(int32_t) + log_entry.key.size()
                        + sizeof(int32_t) + log_entry.value.size()
                        + sizeof(int64_t);
    buf->resize(total_len);
    int32_t key_size = log_entry.key.size();
    int32_t value_size = log_entry.value.size();
    char* p = reinterpret_cast<char*>(& ((*buf)[0]));
    p[0] = static_cast<uint8_t>(log_entry.op);
    p += sizeof(uint8_t);
    memcpy(p, static_cast<const void*>(&key_size), sizeof(int32_t));
    p += sizeof(int32_t);
    memcpy(p, static_cast<const void*>(log_entry.key.data()), log_entry.key.size());
    p += log_entry.key.size();
    memcpy(p, static_cast<const void*>(&value_size), sizeof(int32_t));
    p += sizeof(int32_t);
    memcpy(p, static_cast<const void*>(log_entry.value.data()), log_entry.value.size());
    p += log_entry.value.size();
    memcpy(p, static_cast<const void*>(&log_entry.term), sizeof(int64_t));
}

void BinLogger::LoadLogEntry(const std::string& buf, LogEntry* log_entry) {
    assert(log_entry);  
    const char* p = buf.data();
    int32_t key_size = 0;
    int32_t value_size = 0;
    uint8_t opcode = 0;
    memcpy(static_cast<void*>(&opcode), p, sizeof(uint8_t));
    log_entry->op = static_cast<LogOperation>(opcode);
    p += sizeof(uint8_t);
    memcpy(static_cast<void*>(&key_size), p, sizeof(int32_t));
    log_entry->key.resize(key_size);
    p += sizeof(int32_t);
    memcpy(static_cast<void*>(&log_entry->key[0]), p, key_size);
    p += key_size;
    memcpy(static_cast<void*>(&value_size), p, sizeof(int32_t));
    log_entry->value.resize(value_size);
    p += sizeof(int32_t);
    memcpy(static_cast<void*>(&log_entry->value[0]), p, value_size);
    p += value_size;
    memcpy(static_cast<void*>(&log_entry->term), p , sizeof(int64_t));
}

} //namespace ins 
} //namespace galaxy

