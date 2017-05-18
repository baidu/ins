#include "binlog.h"

#include <assert.h>
#include "common/asm_atomic.h"
#include "common/logging.h"
#include "leveldb/write_batch.h"
#include "utils.h"

namespace galaxy {
namespace ins {

const std::string log_dbname = "#binlog";
const std::string length_tag = "#BINLOG_LEN#";

BinLogger::BinLogger(const std::string& data_dir,
                     bool compress,
                     int32_t block_size,
                     int32_t write_buffer_size) : db_(NULL),
                                                  length_(0),
                                                  last_log_term_(-1) {
    bool ok = ins_common::Mkdirs(data_dir.c_str());
    if (!ok) {
        LOG(FATAL, "failed to create dir :%s", data_dir.c_str());
        abort();
    }
    std::string full_name = data_dir + "/" + log_dbname;
    leveldb::Options options;
    options.create_if_missing = true;
    if (compress) {
        options.compression = leveldb::kSnappyCompression;
        LOG(INFO, "enable snappy compress for binlog");
    }
    options.write_buffer_size = write_buffer_size;
    options.block_size = block_size;
    LOG(INFO, "[binlog]: block_size: %d, writer_buffer_size: %d", 
        options.block_size,
        options.write_buffer_size);
    leveldb::Status status = leveldb::DB::Open(options, full_name, &db_);
    if (!status.ok()) {
        LOG(FATAL, "failed to open db %s err %s", 
                   full_name.c_str(), status.ToString().c_str()); 
        assert(status.ok());
    }
    std::string value;
    status = db_->Get(leveldb::ReadOptions(), length_tag, &value);
    if (status.ok() && !value.empty()) {
        length_ = StringToInt(value);
        if (length_ > 0) {
            LogEntry log_entry;
            bool slot_ok = ReadSlot(length_ - 1, &log_entry);
            assert(slot_ok);
            last_log_term_ = log_entry.term;
        }
    }
}

BinLogger::~BinLogger() {
    delete db_;
}

int64_t BinLogger::GetLength() {
    MutexLock lock(&mu_);
    return length_;
}

void BinLogger::GetLastLogIndexAndTerm(int64_t* last_log_index, int64_t* last_log_term) {
    MutexLock lock(&mu_);
    *last_log_index = length_ - 1;
    *last_log_term = last_log_term_;
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

bool BinLogger::RemoveSlot(int64_t slot_index) {
    std::string value;
    std::string key = IntToString(slot_index);
    leveldb::Status status = db_->Get(leveldb::ReadOptions(), key, &value);
    if (!status.ok()) {
        return false;
    }
    status = db_->Delete(leveldb::WriteOptions(), key);
    if (status.ok()) {
        return true;
    } else {
        return false;
    }
}

bool BinLogger::RemoveSlotBefore(int64_t slot_gc_index) {
    db_->SetNexusGCKey(slot_gc_index);
    return true;
}

bool BinLogger::ReadSlot(int64_t slot_index, LogEntry* log_entry) {
    std::string value;
    std::string key = IntToString(slot_index);
    leveldb::Status status = db_->Get(leveldb::ReadOptions(), key, &value);
    if (status.ok()) {
        LoadLogEntry(value, log_entry);
        return true;
    } else if (status.IsNotFound()) {
        return false;
    }
    abort();
}

void BinLogger::AppendEntryList(
    const ::google::protobuf::RepeatedPtrField< ::galaxy::ins::Entry >& entries
) {
    leveldb::WriteBatch batch;
    {
        MutexLock lock(&mu_);
        int64_t cur_index = length_;
        std::string next_index = IntToString(length_ + entries.size());
        for(int i = 0; i < entries.size(); i++) {
            LogEntry log_entry;
            std::string buf;
            log_entry.op = entries.Get(i).op();
            log_entry.user = entries.Get(i).user();
            log_entry.key = entries.Get(i).key();
            log_entry.value = entries.Get(i).value();
            log_entry.term = entries.Get(i).term();
            last_log_term_ =  log_entry.term;
            DumpLogEntry(log_entry, &buf);
            batch.Put(IntToString(cur_index + i), buf);
        }
        batch.Put(length_tag, next_index);
        leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);
        assert(status.ok());
        length_ += entries.size();
    }
}

void BinLogger::AppendEntry(const LogEntry& log_entry) {
    std::string buf;
    DumpLogEntry(log_entry, &buf);
    std::string cur_index;
    std::string next_index;
    {
        MutexLock lock(&mu_);
        cur_index = IntToString(length_);
        next_index = IntToString(length_ + 1);
        leveldb::WriteBatch batch;
        batch.Put(cur_index, buf);
        batch.Put(length_tag, next_index);
        leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);
        assert(status.ok());
        length_++;
        last_log_term_ = log_entry.term;
    }
}

void BinLogger::Truncate(int64_t trunk_slot_index) {
    if (trunk_slot_index < -1) {
        trunk_slot_index = -1;
    }

    {
        MutexLock lock(&mu_);
        length_ = trunk_slot_index + 1;
        leveldb::Status status = db_->Put(leveldb::WriteOptions(), 
                                          length_tag, IntToString(length_));
        assert(status.ok());
        if (length_ > 0) {
            LogEntry log_entry;
            bool slot_ok = ReadSlot(length_ - 1, &log_entry);
            assert(slot_ok);
            last_log_term_ = log_entry.term;
        }
    }
}

void BinLogger::DumpLogEntry(const LogEntry& log_entry, std::string* buf) {
    assert(buf);
    int32_t total_len = sizeof(uint8_t) 
                        + sizeof(int32_t) + log_entry.user.size()
                        + sizeof(int32_t) + log_entry.key.size()
                        + sizeof(int32_t) + log_entry.value.size()
                        + sizeof(int64_t);
    buf->resize(total_len);
    int32_t user_size = log_entry.user.size();
    int32_t key_size = log_entry.key.size();
    int32_t value_size = log_entry.value.size();
    char* p = reinterpret_cast<char*>(& ((*buf)[0]));
    p[0] = static_cast<uint8_t>(log_entry.op);
    p += sizeof(uint8_t);
    memcpy(p, static_cast<const void*>(&user_size), sizeof(int32_t));
    p += sizeof(int32_t);
    memcpy(p, static_cast<const void*>(log_entry.user.data()), log_entry.user.size());
    p += log_entry.user.size();
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
    int32_t user_size = 0;
    int32_t key_size = 0;
    int32_t value_size = 0;
    uint8_t opcode = 0;
    memcpy(static_cast<void*>(&opcode), p, sizeof(uint8_t));
    log_entry->op = static_cast<LogOperation>(opcode);
    p += sizeof(uint8_t);
    memcpy(static_cast<void*>(&user_size), p, sizeof(int32_t));
    log_entry->user.resize(user_size);
    p += sizeof(int32_t);
    memcpy(static_cast<void*>(&log_entry->user[0]), p, user_size);
    p += user_size;
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

