#ifndef PHP_TIDEWAYS_XHPROF_H
#define PHP_TIDEWAYS_XHPROF_H

extern zend_module_entry tideways_xhprof_module_entry;
#define phpext_tideways_xhprof_ptr &tideways_xhprof_module_entry

#define PHP_TIDEWAYS_XHPROF_VERSION "5.0.0"
#define TIDEWAYS_XHPROF_CALLGRAPH_COUNTER_SIZE 1024
#define TIDEWAYS_XHPROF_CALLGRAPH_SLOTS 8192
#define TIDEWAYS_XHPROF_CLOCK_CGT 0
#define TIDEWAYS_XHPROF_CLOCK_GTOD 1
#define TIDEWAYS_XHPROF_CLOCK_TSC 2
#define TIDEWAYS_XHPROF_CLOCK_MACH 3
#define TIDEWAYS_XHPROF_CLOCK_QPC 4
#define TIDEWAYS_XHPROF_CLOCK_NONE 255

#ifdef ZTS
#include "TSRM.h"
#endif

#if !defined(uint32)
typedef unsigned int uint32;
#endif

#if !defined(uint64)
typedef unsigned long long uint64;
#endif

typedef struct xhprof_frame_t xhprof_frame_t;

typedef struct xhprof_callgraph_bucket_t {
    zend_ulong key;
    zend_string *parent_class;
    zend_string *parent_function;
    int parent_recurse_level;
    zend_string *child_class;
    zend_string *child_function;
    int child_recurse_level;
    struct xhprof_callgraph_bucket_t *next;
    zend_long count;
    zend_long wall_time;
    zend_long cpu_time;
    zend_long memory;
    zend_long memory_peak;
    long int            num_alloc, num_free;
    long int            amount_alloc;
} xhprof_callgraph_bucket;

/* Tracer maintains a stack of entries being profiled.
 *
 * This structure is a convenient place to track start time of a particular
 * profile operation, recursion depth, and the name of the function being
 * profiled. */
struct xhprof_frame_t {
    struct xhprof_frame_t   *previous_frame;        /* ptr to prev entry being profiled */
    zend_string         *function_name;
    zend_string         *class_name;
    uint64              wt_start;           /* start value for wall clock timer */
    uint64              cpu_start;         /* start value for CPU clock timer */
    long int            mu_start;                    /* memory usage */
    long int            pmu_start;              /* peak memory usage */
    long int            num_alloc, num_free;
    long int            amount_alloc;
    int                 recurse_level;
    zend_ulong          hash_code;          /* hash_code for the function name  */
};

ZEND_BEGIN_MODULE_GLOBALS(tideways_xhprof)
    int enabled;
    uint64 start_timestamp;
    uint64 start_time;
    int clock_source;
    zend_bool clock_use_rdtsc;
    double timebase_factor;
    zend_string *root;
    xhprof_frame_t *callgraph_frames;
    xhprof_frame_t *frame_free_list;
    zend_ulong function_hash_counters[TIDEWAYS_XHPROF_CALLGRAPH_COUNTER_SIZE];
    xhprof_callgraph_bucket* callgraph_buckets[TIDEWAYS_XHPROF_CALLGRAPH_SLOTS];
    zend_long flags;
    long int num_alloc;
    long int num_free;
    long int amount_alloc;
ZEND_END_MODULE_GLOBALS(tideways_xhprof)

#if defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_TIDEWAYS_XHPROF_API __attribute__ ((visibility("default")))
#else
#	define PHP_TIDEWAYS_XHPROF_API
#endif

#define TIDEWAYS_LOGO_DATA_URI "data:image/png;base64,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"

#if defined(ZTS) && defined(COMPILE_DL_TIDEWAYS_XHPROF)
ZEND_TSRMLS_CACHE_EXTERN()
#endif

#endif	/* PHP_TIDEWAYS_XHPROF_H */

