#include "util.h"
#include <sys/time.h> // Include the header file for gettimeofday

uint64_t GetCuurentTimeMs() {
    struct timeval tm;
    gettimeofday(&tm, 0);
    return tm.tv_sec * 1000ul + tm.tv_usec / 1000;
}

uint64_t GetCuurentTimeUs() {
    struct timeval tu;
    gettimeofday(&tu, 0);
    return tu.tv_sec * 1000000ul + tu.tv_usec;
}
