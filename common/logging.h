// Copyright (c) 2014, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yanshiguang02@baidu.com

#ifndef  BAIDU_COMMON_LOGGING_H_
#define  BAIDU_COMMON_LOGGING_H_

#include <sstream>
#include <sofa/pbrpc/common.h>

namespace ins_common {

enum LogLevel {
    DEBUG = 2,
    INFO = 4,
    WARNING = 8,
    FATAL = 16,
};

void SetLogLevel(int level);
bool SetLogFile(const char* path, bool append = false);
bool SetWarningFile(const char* path, bool append = false);
bool SetLogSize(int size); // in MB
bool SetLogCount(int count);
bool SetLogSizeLimit(int size); // in MB

void Log(int level, const char* fmt, ...);
// handle sofa-pbrpc logging
void RpcLogHandler(sofa::pbrpc::LogLevel level, const char* filename, int line,
                   const char* fmt, va_list ap);

class LogStream {
public:
    LogStream(int level);
    template<class T>
    LogStream& operator<<(const T& t) {
        oss_ << t;
        return *this;
    }
    ~LogStream();
private:
    int level_;
    std::ostringstream oss_;
};

} // namespace ins_common

using ins_common::DEBUG;
using ins_common::INFO;
using ins_common::WARNING;
using ins_common::FATAL;

#define LOG(level, fmt, args...) ins_common::Log(level, "[%s:%d] " fmt, __FILE__, __LINE__, ##args)
#define LOGS(level) ins_common::LogStream(level)

#endif  // BAIDU_COMMON_LOGGING_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
