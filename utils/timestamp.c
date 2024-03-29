// Returns timestamps for logging.

#include <inttypes.h>
#include <sys/time.h>
#define _POSIX_C_SOURCE 199309L
#include <time.h>

// #define CLOCK_SOURCE    CLOCK_MONOTONIC_FAST
// #define CLOCK_SOURCE    CLOCK_MONOTONIC
#define CLOCK_SOURCE    CLOCK_MONOTONIC_PRECISE

/// Convert seconds to milliseconds
#define SEC_TO_MS(sec) ((sec)*1000)
/// Convert seconds to microseconds
#define SEC_TO_US(sec) ((sec)*1000000)
/// Convert seconds to nanoseconds
#define SEC_TO_NS(sec) ((sec)*1000000000)

/// Convert nanoseconds to seconds
#define NS_TO_SEC(ns)   ((ns)/1000000000)
/// Convert nanoseconds to milliseconds
#define NS_TO_MS(ns)    ((ns)/1000000)
/// Convert nanoseconds to microseconds
#define NS_TO_US(ns)    ((ns)/1000)

/// Get a time stamp in milliseconds.
uint64_t millis() {
    struct timespec ts;
    clock_gettime(CLOCK_SOURCE, &ts);
    uint64_t ms = SEC_TO_MS((uint64_t)ts.tv_sec) + NS_TO_MS((uint64_t)ts.tv_nsec);
    return ms;
}

/// Get a time stamp in microseconds.
uint64_t micros() {
    struct timespec ts;
    clock_gettime(CLOCK_SOURCE, &ts);
    uint64_t us = SEC_TO_US((uint64_t)ts.tv_sec) + NS_TO_US((uint64_t)ts.tv_nsec);
    return us;
}

/// Get a time stamp in nanoseconds.
uint64_t nanos() {
    struct timespec ts;
    clock_gettime(CLOCK_SOURCE, &ts);
    uint64_t ns = SEC_TO_NS((uint64_t)ts.tv_sec) + (uint64_t)ts.tv_nsec;
    return ns;
}
