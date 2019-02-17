#ifndef TRACER_TIMER_H
#define TRACER_TIMER_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef PHP_WIN32
#include "win32/time.h"
#include "win32/unistd.h"
#include "win32/getrusage.h"
#else

#include <sys/time.h>
#include <sys/resource.h>
#endif

#if __APPLE__
#include <mach/mach_init.h>
#include <mach/mach_time.h>
#endif

static zend_always_inline uint64 current_timestamp() {
    struct timeval tv;

    if (gettimeofday(&tv, NULL)) {
        php_error(E_ERROR, "tracer: Cannot acquire gettimeofday");
        return 0;
    }

    return 1000 * (uint64) tv.tv_sec + (uint64) tv.tv_usec / 1000;
}

/**
 * Get the current wallclock timer
 *
 * @return 64 bit unsigned integer
 * @author cjiang
 */
static zend_always_inline uint64 time_milliseconds(int source, double timebase_factor) {
#ifdef __APPLE__
    return mach_absolute_time() / timebase_factor;
#elif defined(PHP_WIN32)

    LARGE_INTEGER count;

    if (!QueryPerformanceCounter(&count)) {
        return 0;
    }

    return (double)(count.QuadPart) / timebase_factor;
#else
    struct timespec s;
    uint32 a, d;
    uint64 val;
#if defined(__i386__)
    int64_t ret;
#endif

    switch (source) {
#if HAVE_CLOCK_GETTIME
        case TIDEWAYS_XHPROF_CLOCK_CGT:
            if (clock_gettime(CLOCK_MONOTONIC, &s) == 0) {
                return s.tv_sec * 1000000 + s.tv_nsec / 1000;
            } else {
                struct timeval now;
                if (gettimeofday(&now, NULL) == 0) {
                    return now.tv_sec * 1000000 + now.tv_usec;
                }
            }
            return 0;
#elif HAVE_GETTIMEOFDAY
        case TIDEWAYS_XHPROF_CLOCK_GTOD:
            struct timeval now;
            if (gettimeofday(&now, NULL) == 0) {
                return now.tv_sec * 1000000 + now.tv_usec;
            }
            return 0;
#endif
        case TIDEWAYS_XHPROF_CLOCK_TSC:
#if defined(__i386__)
            __asm__ volatile("rdtsc" : "=A"(ret));
            return ret;
#elif defined(__x86_64__) || defined(__amd64__)
            asm volatile("rdtsc" : "=a" (a), "=d" (d));
            (val) = ((uint64)a) | (((uint64)d)<<32);
#elif defined(__powerpc__) || defined(__ppc__)
            asm volatile ("mftb %0" : "=r" (val));
#else
#error You need to define CycleTimer for your OS and CPU
#endif
            return val / timebase_factor;

        default:
            return 0;
    }
#endif
}

/**
 * Get time delta in microseconds.
 */
static long get_us_interval(struct timeval *start, struct timeval *end)
{
    return (((end->tv_sec - start->tv_sec) * 1000000)
            + (end->tv_usec - start->tv_usec));
}

/**
 * Get the timebase factor necessary to divide by in time_milliseconds()
 */
static zend_always_inline double get_timebase_factor(int source)
{
#ifdef __APPLE__
    mach_timebase_info_data_t sTimebaseInfo;
    (void) mach_timebase_info(&sTimebaseInfo);

    return (sTimebaseInfo.numer / sTimebaseInfo.denom) * 1000;
#elif defined(PHP_WIN32)
    unsigned __int64 frequency;

    if (!QueryPerformanceFrequency( (LARGE_INTEGER*)&frequency)) {
        zend_error(E_ERROR, "QueryPerformanceFrequency");
    }

    return (double)frequency/1000000.0;
#else
    struct timeval start;
    struct timeval end;
    uint64 tsc_start;
    uint64 tsc_end;
    volatile int i;

    switch (source) {
        case TIDEWAYS_XHPROF_CLOCK_TSC:

            if (gettimeofday(&start, 0)) {
                perror("gettimeofday");
                return 0.0;
            }

            tsc_start  = time_milliseconds(source, 1.0);
            /* Busy loop for 5 miliseconds. */
            do {
                for (i = 0; i < 1000000; i++);
                    if (gettimeofday(&end, 0)) {
                        perror("gettimeofday");
                        return 0.0;
                    }
                tsc_end = time_milliseconds(source, 1.0);
            } while (get_us_interval(&start, &end) < 5000);

            return (tsc_end - tsc_start) * 1.0 / (get_us_interval(&start, &end));
        default:
            return 1.0;
    }
#endif
}

/**
 * Get the current real CPU clock timer
 */
static uint64 cpu_timer() {
    struct rusage ru;
#if defined(CLOCK_PROCESS_CPUTIME_ID)
    struct timespec s;

    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &s) == 0) {
        return s.tv_sec * 1000000 + s.tv_nsec / 1000;
    }
#endif

    if (getrusage(RUSAGE_SELF, &ru) == 0) {
        return ru.ru_utime.tv_sec * 1000000 + ru.ru_utime.tv_usec +
            ru.ru_stime.tv_sec * 1000000 + ru.ru_stime.tv_usec;
    }

    return 0;
}

#endif

