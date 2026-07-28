#define _GNU_SOURCE
#include <time.h>
#include "ls_utils.h"

/* Helper function to get the current time, uses CLOCK_MONOTONIC so should be time since boot */
uint64_t now_ms()
{
    struct timespec ts;
    if(clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
        return UINT64_MAX;
    }
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}
