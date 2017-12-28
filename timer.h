#ifndef TRACER_TIMER_H
#define TRACER_TIMER_H

#include "config.h"
#include "sys/time.h"

static zend_always_inline uint64 current_timestamp() {
    struct timeval tv;

    if (gettimeofday(&tv, NULL)) {
        php_error(E_ERROR, "tracer: Cannot acquire gettimeofday");
        zend_bailout();
    }

    return 1000 * (uint64) tv.tv_sec + (uint64) tv.tv_usec / 1000;
}

/**
 * Get the current wallclock timer
 *
 * @return 64 bit unsigned integer
 * @author cjiang
 */
static zend_always_inline uint64 time_milliseconds() {
#ifdef __APPLE__
    return mach_absolute_time() / TXRG(timebase_factor);
#else
    struct timespec s;

#if HAVE_CLOCK_GETTIME
    if (clock_gettime(CLOCK_MONOTONIC, &s) == 0) {
        return s.tv_sec * 1000000 + s.tv_nsec / 1000;
    } else {
        struct timeval now;
        if (gettimeofday(&now, NULL) == 0) {
            return now.tv_sec * 1000000 + now.tv_usec;
        }
    }
#elif HAVE_GETTIMEOFDAY
    struct timeval now;
    if (gettimeofday(&now, NULL) == 0) {
        return now.tv_sec * 1000000 + now.tv_usec;
    }
#endif
    return 0;
#endif
}

/**
 * Get the timebase factor necessary to divide by in time_milliseconds()
 */
static zend_always_inline double get_timebase_factor()
{
#ifdef __APPLE__
    mach_timebase_info_data_t sTimebaseInfo;
    (void) mach_timebase_info(&sTimebaseInfo);

    return (sTimebaseInfo.numer / sTimebaseInfo.denom) * 1000;
#else
    return 1.0;
#endif
}

/**
 * Get the current real CPU clock timer
 */
static uint64 cpu_timer() {
#if defined(CLOCK_PROCESS_CPUTIME_ID)
    struct timespec s;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &s);

    return s.tv_sec * 1000000 + s.tv_nsec / 1000;
#else
    struct rusage ru;

    if (getrusage(RUSAGE_SELF, &ru) == 0) {
        return ru.ru_utime.tv_sec * 1000000 + ru.ru_utime.tv_usec +
            ru.ru_stime.tv_sec * 1000000 + ru.ru_stime.tv_usec;
    }

    return 0;
#endif
}

#endif

