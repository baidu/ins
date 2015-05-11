#include "binlog.h"

namespace galaxy {
namespace ins {


BinLogger::BinLogger(const std::string& data_dir) : log_file_(NULL),
                                                    slot_count_(0) {

}

BinLogger::~BinLogger() {

}

bool BinLogger::ReadUntil(int64_t end_slot_index, 
                          boost::function<void (const LogEntry& log_entry)> reader) {
    return true;
}

int64_t BinLogger::GetLength() {
    return slot_count_;
}

bool BinLogger::ReadSlot(int64_t slot_index, LogEntry* log_entry) {
    return true;
}

void BinLogger::AppendEntry(const LogEntry& log_entry) {

}

void BinLogger::Truncate(int64_t start_slot_index) {

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
	char* p = reinterpret_cast<char*>((*buf)[0]);
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
	memcpy(static_cast<void*>(&log_entry->op), p, sizeof(uint8_t));
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

