// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: zhangkai17@baidu.com

#ifndef COMMON_SPIN_LOCK_H_
#define COMMON_SPIN_LOCK_H_

#include <pthread.h>

namespace ins_common {

class SpinLock {
public:
    inline SpinLock() {
        pthread_spin_init(&spinlock_, PTHREAD_PROCESS_PRIVATE);
    }
    virtual inline ~SpinLock() {
        pthread_spin_destroy(&spinlock_);
    }
    inline int Lock() {
        return pthread_spin_lock(&spinlock_);
    }
    inline int Unlock() {
        return pthread_spin_unlock(&spinlock_);
    }
    inline int TryLock() {
        return pthread_spin_trylock(&spinlock_);
    }

private:
    pthread_spinlock_t spinlock_;
};

} // namespace common

using ins_common::SpinLock;

#endif // COMMON_SPIN_LOCK_H_
