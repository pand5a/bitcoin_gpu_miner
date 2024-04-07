//
// Created by fly on 2020/6/17.
//

#include "time_utils.h"
#include <iostream>
#include <iomanip>
#include <math.h>
#include <boost/date_time/posix_time/posix_time.hpp>
//#include <boost/thread.hpp>

TimeUtils::TimeUtils() {
   Reset();
}

void TimeUtils::Reset() {
    _start = system_clock::now();
}

void TimeUtils::End(const char* str, size_t hashNum) {
    auto end   = system_clock::now();
    auto duration = duration_cast<microseconds>(end - _start);
    auto second = double(duration.count()) * microseconds::period::num / microseconds::period::den;
    std::cout << "\033[31m" <<  str <<  "消耗时间: " << ((1000.0 * second + 0.5) / 1000.0) << " 秒" << std::endl;

    if (hashNum > 0) {
        auto hashRate = hashNum / second / 1000;
        std::cout << "\033[31m" <<  "　　算力: " << std::fixed << setprecision(3)  << hashRate << " KH/s" << std::endl;
    }
}

static std::atomic<int64_t> nMockTime(0); //!< For unit testing
int64_t GetTime()
{
    int64_t mocktime = nMockTime.load(std::memory_order_relaxed);
    if (mocktime) return mocktime;

    time_t now = time(nullptr);
    assert(now > 0);
    return now;
}

int64_t GetTimeMicros()
{
    int64_t now = (boost::posix_time::microsec_clock::universal_time() -
                   boost::posix_time::ptime(boost::gregorian::date(1970,1,1))).total_microseconds();
    assert(now > 0);
    return now;
}


//}


