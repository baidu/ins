#ifndef GALAXY_SDK_BINGLOG_H_
#define GALAXY_SDK_BINGLOG_H_

#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <boost/function.hpp>

namespace galaxy {
namespace ins {

enum LogOperation {
    kPut = 1,
    kDel = 2
};

struct LogEntry {
    LogOperation op;
    std::string key;
    std::string value;
    int64_t term;
};

class BinLogger {
public:
    BinLogger(const std::string& data_dir);
    ~BinLogger();
    int64_t GetLength();
    bool ReadUntil(int64_t end_slot_index, boost::function<void (const LogEntry& log_entry)>);
    bool ReadSlot(int64_t slot_index, LogEntry* log_entry);
    void AppendEntry(const LogEntry& log_entry);
    void Truncate(int64_t start_slot_index);
private:
    void DumpLogEntry(const LogEntry& log_entry, std::string* buf);
    void LoadLogEntry(const std::string& buf, LogEntry* log_entry);
private:
    FILE* log_data_file_;
    FILE* log_index_file_;
};

} //namespace ins 
} //namespace galaxy

#endif
