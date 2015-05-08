#ifndef GALAXY_SDK_BINGLOG_H_
#define GALAXY_SDK_BINGLOG_H_

#include <string>
#include <stdio.h>
#include <stdlib.h>

namespace galaxy {
namespace ins {

struct LogEntry {
    std::string key;
    std::string value;
    int64_t term;
};

class BinLogger {
public:
    BinLogger(const std::string& data_dir);
    ~BinLogger();
    int64_t GetLength();
    bool ReadSlot(int64_t slot_index, LogEntry* log_entry);
    void AppendEntry(const LogEntry& log_entry);
    void Truncate(int64_t start_slot_index);
private:
    FILE* log_file_;
};

} //namespace ins 
} //namespace galaxy

#endif
