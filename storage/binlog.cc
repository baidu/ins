#include "binlog.h"

#include "common/asm_atomic.h"
#include "common/logging.h"

namespace galaxy {
namespace ins {

const std::string log_data_file_name = "log.data";
const std::string log_index_file_name = "log.index";

BinLogger::BinLogger(const std::string& data_dir) : log_data_file_(NULL),
                                                    log_index_file_(NULL) {
    std::string log_data_full_name = data_dir + "/" + log_data_file_name;
    std::string log_index_full_name = data_dir + "/" + log_index_file_name;
    log_data_file_ =  fopen(log_data_full_name.c_str(), "a+");
    assert(log_data_file_);
    log_index_file_ = fopen(log_index_full_name.c_str(), "a+");
    assert(log_index_file_);
}

BinLogger::~BinLogger() {
    fclose(log_data_file_);
    fclose(log_index_file_);
}

bool BinLogger::ReadUntil(int64_t end_slot_index, 
                          boost::function<void (const LogEntry& log_entry)> reader) {
    int64_t slot_count = GetLength();
    if (end_slot_index >= slot_count) {
        LOG(FATAL, "invalid end_slot_index: %ld >= %ld", end_slot_index, slot_count);
        return false;
    }
    fseek(log_data_file_, 0 , SEEK_SET);
    LogEntry log_entry;
    int64_t entry_len = 0;
    if (fread(&entry_len, sizeof(int64_t), 1, log_data_file_) != 1) {
        LOG(FATAL, "faild to read file %s", log_data_file_name.c_str());
        abort();
    }
    std::string buf;
    buf.resize(entry_len);
    if (fread(&buf[0], entry_len, 1, log_data_file_) !=1 ) {
        LOG(FATAL, "faild to read file %s", log_data_file_name.c_str());
        abort();
    }
    LoadLogEntry(buf, &log_entry);
    reader(log_entry);
    return true;
}

int64_t BinLogger::GetLength() {
    MutexLock lock(&mu_);
    fseek(log_index_file_, 0 , SEEK_END);
    int64_t index_file_size = ftell(log_index_file_);
    return index_file_size / sizeof(int64_t);
}

bool BinLogger::ReadSlot(int64_t slot_index, LogEntry* log_entry) {
    MutexLock lock(&mu_);
    int64_t offset = slot_index * sizeof(int64_t);
    if (fseek(log_index_file_, offset, SEEK_SET) < 0) {
        LOG(FATAL, "seek file failed: %s", log_index_file_name.c_str());
        return false;
    }
    int64_t data_offset = 0 ;
    if (fread(&data_offset, sizeof(int64_t), 1, log_index_file_) != 1) {
        LOG(FATAL, "faild to read file %s", log_index_file_name.c_str());
        abort();
    }
    if (fseek(log_data_file_, data_offset, SEEK_SET) < 0) {
        LOG(FATAL, "seek file failed: %s", log_data_file_name.c_str());
        return false;
    }
    int64_t entry_len = 0;
    if (fread(&entry_len, sizeof(int64_t), 1, log_data_file_) != 1) {
        LOG(FATAL, "faild to read file %s", log_data_file_name.c_str());
        abort();
    }
    std::string buf;
    buf.resize(entry_len);
    if (fread(&buf[0], entry_len, 1, log_data_file_) !=1 ) {
        LOG(FATAL, "faild to read file %s", log_data_file_name.c_str());
        abort();
    }
    LoadLogEntry(buf, log_entry);
    return true;
}

void BinLogger::AppendEntry(const LogEntry& log_entry) {
    MutexLock lock(&mu_);
    std::string buf;
    DumpLogEntry(log_entry, &buf);
    int64_t entry_len = buf.size();
    fseek(log_data_file_, 0, SEEK_END);
    int64_t data_cur_size = ftell(log_data_file_);

    if (fwrite(&entry_len, sizeof(int64_t), 1, log_data_file_) != 1) {
        LOG(FATAL, "write file %s faild", log_data_file_name.c_str());
        abort();
    }
    if (fwrite(&buf[0], buf.size(), 1, log_data_file_) != 1) {
        LOG(FATAL, "write file %s faild", log_data_file_name.c_str());
        abort();
    }
    if (fflush(log_data_file_) != 0) {
        LOG(FATAL, "failed to flush %s", log_data_file_name.c_str());
        abort();
    }
    if (fwrite(&data_cur_size, sizeof(int64_t), 1, log_index_file_) != 1) {
        LOG(FATAL, "write file %s faild", log_index_file_name.c_str());
        abort();
    }
    if (fflush(log_index_file_) != 0) {
        LOG(FATAL, "failed to flush %s", log_index_file_name.c_str());
        abort();
    }
}

void BinLogger::Truncate(int64_t trunk_slot_index) {
    int64_t offset = trunk_slot_index * sizeof(int64_t);
    if (offset < 0 ) {
        if (fflush(log_data_file_) != 0) {
            LOG(FATAL, "failed to flush %s", log_data_file_name.c_str());
            abort();
        }
        if (fflush(log_index_file_) != 0) {
            LOG(FATAL, "failed to flush %s", log_index_file_name.c_str());
            abort();
        }
        if (ftruncate(fileno(log_index_file_), 0) != 0) {
            LOG(FATAL, "failed to truncate %s", log_index_file_name.c_str());
            abort();
        }
        if (ftruncate(fileno(log_data_file_), 0) != 0 ){
            LOG(FATAL, "failed to truncate %s", log_data_file_name.c_str());
            abort();
        }    
        return;
    }
    if (fseek(log_index_file_, offset, SEEK_SET) < 0) {
        LOG(FATAL, "seek file failed: %s", log_index_file_name.c_str());
        abort();
    }
    int64_t data_offset = 0 ;
    if (fread(&data_offset, sizeof(int64_t), 1, log_index_file_) != 1) {
        LOG(FATAL, "faild to read file %s", log_index_file_name.c_str());
        abort();
    }
    if (fseek(log_data_file_, data_offset, SEEK_SET) < 0) {
        LOG(FATAL, "seek file failed: %s", log_data_file_name.c_str());
        abort();
    }
    int64_t entry_len = 0;
    if (fread(&entry_len, sizeof(int64_t), 1, log_data_file_) != 1) {
        LOG(FATAL, "faild to read file %s", log_data_file_name.c_str());
        abort();
    }
    if (fflush(log_data_file_) != 0) {
        LOG(FATAL, "failed to flush %s", log_data_file_name.c_str());
        abort();
    }
    if (fflush(log_index_file_) != 0) {
        LOG(FATAL, "failed to flush %s", log_index_file_name.c_str());
        abort();
    }
    if (ftruncate(fileno(log_index_file_), offset + sizeof(int64_t)) != 0) {
        LOG(FATAL, "failed to truncate %s", log_index_file_name.c_str());
        abort();
    }
    if (ftruncate(fileno(log_data_file_), 
                         data_offset + entry_len + sizeof(entry_len)) != 0 ){
        LOG(FATAL, "failed to truncate %s", log_data_file_name.c_str());
        abort();
    }
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

