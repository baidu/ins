// Copyright (c) 2014, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yanshiguang02@baidu.com

#ifndef  BAIDU_COMMON_COUNTER_H_
#define  BAIDU_COMMON_COUNTER_H_

#include "asm_atomic.h"

namespace ins_common {

class Counter {
    volatile int64_t val_;
public:
    Counter() : val_(0) {}
    int64_t Add(int64_t v) {
        return atomic_add_ret_old64(&val_, v) + v;
    }
    int64_t Sub(int64_t v) {
        return atomic_add_ret_old64(&val_, -v) - v;
    }
    int64_t Inc() {
        return atomic_add_ret_old64(&val_, static_cast<int64_t>(1)) + 1;
    }
    int64_t Dec() {
        return atomic_add_ret_old64(&val_,static_cast<int64_t>(-1)) - 1;
    }
    int64_t Get() const {
        return val_;
    }
    int64_t Set(int64_t v) {
        return atomic_swap(&val_, v);
    }
    int64_t Clear() {
        return atomic_swap(&val_, static_cast<int64_t>(0));
    }
};

} // namespace ins_common

using ins_common::Counter;

#endif  // BAIDU_COMMON_COUNTER_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
