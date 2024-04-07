//
// Created by fly on 2020/6/17.
//

#ifndef BITCOIN_GPU_MINER_TIME_UTILS_H
#define BITCOIN_GPU_MINER_TIME_UTILS_H

#include <chrono>
using namespace std;
using namespace chrono;

class TimeUtils {
private:
    system_clock::time_point _start;
public:
    TimeUtils();

    void Reset();
    void End(const char* str = nullptr, size_t hashNum = 0);
    void End(size_t hashNum = 0) {
        End(nullptr, hashNum);
    }
//    void HashRate(size_t hashNum);

};

int64_t GetTime();
/** Returns the system time (not mockable) */
int64_t GetTimeMicros();

#endif //BITCOIN_GPU_MINER_TIME_UTILS_H
