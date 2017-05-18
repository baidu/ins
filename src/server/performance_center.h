#ifndef _GALAXY_INS_PERFORMANCE_CENTER_H_
#define _GALAXY_INS_PERFORMANCE_CENTER_H_

#include <boost/circular_buffer.hpp>
#include "common/counter.h"
#include "common/thread_pool.h"
#include "common/spin_lock.h"

namespace galaxy {
namespace ins {

class PerformanceCenter {
public:
    PerformanceCenter(int buffer_size);
    virtual ~PerformanceCenter() { }

    void Put() { put_counter_.Inc(); }
    void Get() { get_counter_.Inc(); }
    void Delete() { delete_counter_.Inc(); }
    void Scan() { scan_counter_.Inc(); }
    void KeepAlive() { keepalive_counter_.Inc(); }
    void Lock() { lock_counter_.Inc(); }
    void Unlock() { unlock_counter_.Inc(); }
    void Watch() { watch_counter_.Inc(); }

    int64_t CurrentPut() { return put_counter_.Get(); }
    int64_t CurrentGet() { return get_counter_.Get(); }
    int64_t CurrentDelete() { return delete_counter_.Get(); }
    int64_t CurrentScan() { return scan_counter_.Get(); }
    int64_t CurrentKeepAlive() { return keepalive_counter_.Get(); }
    int64_t CurrentLock() { return lock_counter_.Get(); }
    int64_t CurrentUnlock() { return unlock_counter_.Get(); }
    int64_t CurrentWatch() { return watch_counter_.Get(); }

    int64_t AveragePut() { return CalcAverageOfBuffer(put_buffer_); }
    int64_t AverageGet() { return CalcAverageOfBuffer(get_buffer_); }
    int64_t AverageDelete() { return CalcAverageOfBuffer(delete_buffer_); }
    int64_t AverageScan() { return CalcAverageOfBuffer(scan_buffer_); }
    int64_t AverageKeepAlive() { return CalcAverageOfBuffer(keepalive_buffer_); }
    int64_t AverageLock() { return CalcAverageOfBuffer(lock_buffer_); }
    int64_t AverageUnlock() { return CalcAverageOfBuffer(unlock_buffer_); }
    int64_t AverageWatch() { return CalcAverageOfBuffer(watch_buffer_); }

private:
    void TickTock();
    int64_t CalcAverageOfBuffer(const boost::circular_buffer<Counter>& buffer);

private:
    Counter put_counter_;
    Counter get_counter_;
    Counter delete_counter_;
    Counter scan_counter_;
    Counter keepalive_counter_;
    Counter lock_counter_;
    Counter unlock_counter_;
    Counter watch_counter_;
    boost::circular_buffer<Counter> put_buffer_;
    boost::circular_buffer<Counter> get_buffer_;
    boost::circular_buffer<Counter> delete_buffer_;
    boost::circular_buffer<Counter> scan_buffer_;
    boost::circular_buffer<Counter> keepalive_buffer_;
    boost::circular_buffer<Counter> lock_buffer_;
    boost::circular_buffer<Counter> unlock_buffer_;
    boost::circular_buffer<Counter> watch_buffer_;
    SpinLock lock_;
    ThreadPool ticktock_;
};

}
}

#endif
