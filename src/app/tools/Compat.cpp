// GCC 13 的 MinGW 头缺 time64 声明，libstdc++ 在 -O2 下会引用，这里补一份实现。
#include "app/tools/tools.h"

#if defined(_WIN32) && __GNUC__ < 14
#include <windows.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"

extern "C" {

struct RmlTimespec {
    long long tv_sec;
    long tv_nsec;
};

[[used]]
int clock_gettime64(int clock_id, RmlTimespec* ts) {
    if (ts == nullptr) return -1;

    if (clock_id == 0) {  // CLOCK_REALTIME
        FILETIME file_time{};
        GetSystemTimePreciseAsFileTime(&file_time);
        ULARGE_INTEGER ticks{};
        ticks.LowPart = file_time.dwLowDateTime;
        ticks.HighPart = file_time.dwHighDateTime;
        const long long hundred_ns = static_cast<long long>(ticks.QuadPart) - 116444736000000000LL;
        ts->tv_sec = hundred_ns / 10000000LL;
        ts->tv_nsec = static_cast<long>((hundred_ns % 10000000LL) * 100);
        return 0;
    }

    if (clock_id == 1) {  // CLOCK_MONOTONIC
        LARGE_INTEGER frequency{};
        LARGE_INTEGER counter{};
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&counter);
        ts->tv_sec = counter.QuadPart / frequency.QuadPart;
        ts->tv_nsec = static_cast<long>((counter.QuadPart % frequency.QuadPart) * 1000000000LL /
                                        frequency.QuadPart);
        return 0;
    }

    return -1;
}

[[used]]
int nanosleep64(const RmlTimespec* request, RmlTimespec* remaining) {
    if (request == nullptr) return -1;
    if (remaining != nullptr) {
        remaining->tv_sec = 0;
        remaining->tv_nsec = 0;
    }
    const long long ms = request->tv_sec * 1000 + request->tv_nsec / 1000000;
    Sleep(static_cast<DWORD>(ms > 0 ? ms : 0));
    return 0;
}

}  // extern "C"

#pragma GCC diagnostic pop
#endif
