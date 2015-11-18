#include "server/performance_center.h"

#include <boost/bind.hpp>
#include "gflags/gflags.h"

DECLARE_int32(performance_interval);

namespace galaxy {
namespace ins {

PerformanceCenter::PerformanceCenter(int buffer_size) :
        put_buffer_(buffer_size),
        get_buffer_(buffer_size),
        delete_buffer_(buffer_size),
        scan_buffer_(buffer_size),
        keepalive_buffer_(buffer_size),
        lock_buffer_(buffer_size),
        unlock_buffer_(buffer_size),
        watch_buffer_(buffer_size),
        ticktock_(1) {
    ticktock_.AddTask(boost::bind(&PerformanceCenter::TickTock, this));
}

int64_t PerformanceCenter::CalcAverageOfBuffer(const boost::circular_buffer<Counter>& buffer) {
    lock_.Lock();
    size_t size = buffer.size();
    int64_t sum = 0;
    for (size_t i = 0; i < size; ++i) {
        sum += buffer[i].Get();
    }
    lock_.Unlock();
    return sum / size;
}

void PerformanceCenter::TickTock() {
    lock_.Lock();

    Counter temp;
    int64_t value = put_counter_.Get();
    put_counter_.Clear();
    temp.Set(value);
    put_buffer_.push_back(temp);

    value = get_counter_.Get();
    get_counter_.Clear();
    temp.Set(value);
    get_buffer_.push_back(temp);

    value = delete_counter_.Get();
    delete_counter_.Clear();
    temp.Set(value);
    delete_buffer_.push_back(temp);

    value = scan_counter_.Get();
    scan_counter_.Clear();
    temp.Set(value);
    scan_buffer_.push_back(temp);

    value = keepalive_counter_.Get();
    keepalive_counter_.Clear();
    temp.Set(value);
    keepalive_buffer_.push_back(temp);

    value = lock_counter_.Get();
    lock_counter_.Clear();
    temp.Set(value);
    lock_buffer_.push_back(temp);

    value = unlock_counter_.Get();
    unlock_counter_.Clear();
    temp.Set(value);
    unlock_buffer_.push_back(temp);

    value = watch_counter_.Get();
    watch_counter_.Clear();
    temp.Set(value);
    watch_buffer_.push_back(temp);

    lock_.Unlock();

    ticktock_.DelayTask(FLAGS_performance_interval,
            boost::bind(&PerformanceCenter::TickTock, this));
}


}
}

