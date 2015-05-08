#include "binlog.h"

namespace galaxy {
namespace ins {


BinLogger::BinLogger(const std::string& data_dir) {

}

BinLogger::~BinLogger() {

}
int64_t BinLogger::GetLength() {

}

bool BinLogger::ReadSlot(int64_t slot_index, LogEntry* log_entry) {

}

void BinLogger::AppendEntry(const LogEntry& log_entry) {

}

void BinLogger::Truncate(int64_t start_slot_index) {

}

} //namespace ins 
} //namespace galaxy

