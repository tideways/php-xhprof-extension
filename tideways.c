/*
 *  Copyright (c) 2009 Facebook
 *  Copyright (c) 2014 Qafoo GmbH
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef linux
/* To enable CPU_ZERO and CPU_SET, etc.     */
# define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "ext/standard/file.h"
#include "php_tideways.h"
#include "zend_extensions.h"
#include "zend_gc.h"

#include "ext/pcre/php_pcre.h"
#include "ext/standard/url.h"
#include "ext/pdo/php_pdo_driver.h"
#include "zend_stream.h"

#ifdef PHP_TIDEWAYS_HAVE_CURL
#if PHP_VERSION_ID > 50399
#include <curl/curl.h>
#include <curl/easy.h>
#endif
#endif

#ifdef __FreeBSD__
# if __FreeBSD_version >= 700110
#   include <sys/cpuset.h>
#   define cpu_set_t cpuset_t
#   define SET_AFFINITY(pid, size, mask) \
           cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1, size, mask)
#   define GET_AFFINITY(pid, size, mask) \
           cpuset_getaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1, size, mask)
# else
#   error "This version of FreeBSD does not support cpusets"
# endif /* __FreeBSD_version */
#elif __APPLE__
/*
 * Patch for compiling in Mac OS X Leopard
 * @author Svilen Spasov <s.spasov@gmail.com>
 */
#    include <mach/mach_init.h>
#    include <mach/thread_policy.h>
#    include <mach/mach_time.h>

#    define cpu_set_t thread_affinity_policy_data_t
#    define CPU_SET(cpu_id, new_mask) \
        (*(new_mask)).affinity_tag = (cpu_id + 1)
#    define CPU_ZERO(new_mask)                 \
        (*(new_mask)).affinity_tag = THREAD_AFFINITY_TAG_NULL
#   define SET_AFFINITY(pid, size, mask)       \
        thread_policy_set(mach_thread_self(), THREAD_AFFINITY_POLICY, mask, \
                          THREAD_AFFINITY_POLICY_COUNT)
#else
/* For sched_getaffinity, sched_setaffinity */
# include <sched.h>
# define SET_AFFINITY(pid, size, mask) sched_setaffinity(0, size, mask)
# define GET_AFFINITY(pid, size, mask) sched_getaffinity(0, size, mask)
#endif /* __FreeBSD__ */

/**
 * **********************
 * GLOBAL MACRO CONSTANTS
 * **********************
 */

/* Tideways version                           */
#define TIDEWAYS_VERSION       "1.7.2"

/* Fictitious function name to represent top of the call tree. The paranthesis
 * in the name is to ensure we don't conflict with user function names.  */
#define ROOT_SYMBOL                "main()"

/* Size of a temp scratch buffer            */
#define SCRATCH_BUF_LEN            512

/* Hierarchical profiling flags.
 *
 * Note: Function call counts and wall (elapsed) time are always profiled.
 * The following optional flags can be used to control other aspects of
 * profiling.
 */
#define TIDEWAYS_FLAGS_NO_BUILTINS   0x0001 /* do not profile builtins */
#define TIDEWAYS_FLAGS_CPU           0x0002 /* gather CPU times for funcs */
#define TIDEWAYS_FLAGS_MEMORY        0x0004 /* gather memory usage for funcs */
#define TIDEWAYS_FLAGS_NO_USERLAND   0x0008 /* do not profile userland functions */
#define TIDEWAYS_FLAGS_NO_COMPILE    0x0010 /* do not profile require/include/eval */

/* Constant for ignoring functions, transparent to hierarchical profile */
#define TIDEWAYS_MAX_FILTERED_FUNCTIONS  256
#define TIDEWAYS_FILTERED_FUNCTION_SIZE                           \
               ((TIDEWAYS_MAX_FILTERED_FUNCTIONS + 7)/8)
#define TIDEWAYS_MAX_ARGUMENT_LEN 256

#if !defined(uint64)
typedef unsigned long long uint64;
#endif
#if !defined(uint32)
typedef unsigned int uint32;
#endif
#if !defined(uint8)
typedef unsigned char uint8;
#endif

#define register_trace_callback(function_name, cb) zend_hash_update(hp_globals.trace_callbacks, function_name, sizeof(function_name), &cb, sizeof(tw_trace_callback*), NULL);


/**
 * *****************************
 * GLOBAL DATATYPES AND TYPEDEFS
 * *****************************
 */

/* Tideways maintains a stack of entries being profiled. The memory for the entry
 * is passed by the layer that invokes BEGIN_PROFILING(), e.g. the hp_execute()
 * function. Often, this is just C-stack memory.
 *
 * This structure is a convenient place to track start time of a particular
 * profile operation, recursion depth, and the name of the function being
 * profiled. */
typedef struct hp_entry_t {
	char                   *name_hprof;                       /* function name */
	int                     rlvl_hprof;        /* recursion level for function */
	uint64                  tsc_start;         /* start value for TSC counter  */
	long int                mu_start_hprof;                    /* memory usage */
	long int                pmu_start_hprof;              /* peak memory usage */
	struct rusage           ru_start_hprof;             /* user/sys time start */
	struct hp_entry_t      *prev_hprof;    /* ptr to prev entry being profiled */
	uint8                   hash_code;     /* hash_code for the function name  */
	zend_uint				gc_runs; /* number of garbage collection runs */
	zend_uint				gc_collected; /* number of collected items in garbage run */
} hp_entry_t;

typedef struct hp_string {
	char *value;
	size_t length;
} hp_string;

typedef struct hp_function_map {
	char **names;
	uint8 filter[TIDEWAYS_FILTERED_FUNCTION_SIZE];
} hp_function_map;

/* Tideways's global state.
 *
 * This structure is instantiated once.  Initialize defaults for attributes in
 * hp_init_profiler_state() Cleanup/free attributes in
 * hp_clean_profiler_state() */
typedef struct hp_global_t {

	/*       ----------   Global attributes:  -----------       */

	/* Indicates if Tideways is currently enabled */
	int              enabled;

	/* Indicates if Tideways was ever enabled during this request */
	int              ever_enabled;

	int				 prepend_overwritten;

	/* Holds all the Tideways statistics */
	zval            *stats_count;
	zval			*spans;
	uint64			start_time;

	zval			*backtrace;
	zval			*exception;

	/* Top of the profile stack */
	hp_entry_t      *entries;

	/* freelist of hp_entry_t chunks for reuse... */
	hp_entry_t      *entry_free_list;

	/* Function that determines the transaction name and callback */
	hp_string       *transaction_function;
	hp_string		*transaction_name;
	char			*root;

	hp_string		*exception_function;

	/* This array is used to store cpu frequencies for all available logical
	 * cpus.  For now, we assume the cpu frequencies will not change for power
	 * saving or other reasons. If we need to worry about that in the future, we
	 * can use a periodical timer to re-calculate this arrary every once in a
	 * while (for example, every 1 or 5 seconds). */
	double *cpu_frequencies;

	/* The number of logical CPUs this machine has. */
	uint32 cpu_num;

	int invariant_tsc;

	/* The saved cpu affinity. */
	cpu_set_t prev_mask;

	/* The cpu id current process is bound to. (default 0) */
	uint32 cur_cpu_id;

	/* Tideways flags */
	uint32 tideways_flags;

	/* counter table indexed by hash value of function names. */
	uint8  func_hash_counters[256];

	/* Table of filtered function names and their filter */
	int     filtered_type; // 1 = blacklist, 2 = whitelist, 0 = nothing

	hp_function_map *filtered_functions;
	hp_function_map *trace_functions;

	HashTable *trace_callbacks;
	HashTable *span_cache;

	/* Table of functions which allow custom tracing */
	char **trace_function_names;
	uint8   trace_function_filter[TIDEWAYS_FILTERED_FUNCTION_SIZE];

} hp_global_t;

#ifdef PHP_TIDEWAYS_HAVE_CURL
#if PHP_VERSION_ID > 50399
typedef struct hp_curl_t {
	struct {
		char str[CURL_ERROR_SIZE + 1];
		int  no;
	} err;

	void *free;

	struct {
		char *str;
		size_t str_len;
	} hdr;

	void ***thread_ctx;
	CURL *cp;
} hp_curl_t;
#endif
#endif

typedef void (*tw_trace_callback)(char *symbol, void **args, int args_len, zval *object, double start, double end TSRMLS_DC);

/**
 * ***********************
 * GLOBAL STATIC VARIABLES
 * ***********************
 */
/* Tideways global state */
static hp_global_t       hp_globals;

#if PHP_VERSION_ID < 50500
/* Pointer to the original execute function */
static ZEND_DLEXPORT void (*_zend_execute) (zend_op_array *ops TSRMLS_DC);

/* Pointer to the origianl execute_internal function */
static ZEND_DLEXPORT void (*_zend_execute_internal) (zend_execute_data *data,
                           int ret TSRMLS_DC);
#else
/* Pointer to the original execute function */
static void (*_zend_execute_ex) (zend_execute_data *execute_data TSRMLS_DC);

/* Pointer to the origianl execute_internal function */
static void (*_zend_execute_internal) (zend_execute_data *data,
                      struct _zend_fcall_info *fci, int ret TSRMLS_DC);
#endif

/* Pointer to the original compile function */
static zend_op_array * (*_zend_compile_file) (zend_file_handle *file_handle,
                                              int type TSRMLS_DC);

/* Pointer to the original compile string function (used by eval) */
static zend_op_array * (*_zend_compile_string) (zval *source_string, char *filename TSRMLS_DC);

/* error callback replacement functions */
void (*tideways_original_error_cb)(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args);
void tideways_error_cb(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args);

/* Bloom filter for function names to be ignored */
#define INDEX_2_BYTE(index)  (index >> 3)
#define INDEX_2_BIT(index)   (1 << (index & 0x7));


/**
 * ****************************
 * STATIC FUNCTION DECLARATIONS
 * ****************************
 */
static void hp_register_constants(INIT_FUNC_ARGS);

static void hp_begin(long tideways_flags TSRMLS_DC);
static void hp_stop(TSRMLS_D);
static void hp_end(TSRMLS_D);

static inline uint64 cycle_timer();
static double get_cpu_frequency();
static void clear_frequencies();
static int is_invariant_tsc();

static void hp_free_the_free_list();
static hp_entry_t *hp_fast_alloc_hprof_entry();
static void hp_fast_free_hprof_entry(hp_entry_t *p);
static inline uint8 hp_inline_hash(char * str);
static void get_all_cpu_frequencies();
static long get_us_interval(struct timeval *start, struct timeval *end);
static void incr_us_interval(struct timeval *start, uint64 incr);
static inline double get_us_from_tsc(uint64 count, double cpu_frequency);

static void hp_parse_options_from_arg(zval *args);
static void hp_clean_profiler_options_state();

static void hp_exception_function_clear();
static void hp_transaction_function_clear();
static void hp_transaction_name_clear();

static inline zval  *hp_zval_at_key(char  *key, zval  *values);
static inline char **hp_strings_in_zval(zval  *values);
static inline void   hp_array_del(char **name_array);
static inline hp_string *hp_create_string(const char *value, size_t length);
static inline long hp_zval_to_long(zval *z);
static inline hp_string *hp_zval_to_string(zval *z);
static inline zval *hp_string_to_zval(hp_string *str);
static inline void hp_string_clean(hp_string *str);
static char *hp_get_sql_summary(char *sql, int len TSRMLS_DC);
static char *hp_get_file_summary(char *filename, int filename_len TSRMLS_DC);

static inline hp_function_map *hp_function_map_create(char **names);
static inline void hp_function_map_clear(hp_function_map *map);
static inline int hp_function_map_exists(hp_function_map *map, uint8 hash_code, char *curr_func);
static inline int hp_function_map_filter_collision(hp_function_map *map, uint8 hash);

/* {{{ arginfo */
ZEND_BEGIN_ARG_INFO_EX(arginfo_tideways_enable, 0, 0, 0)
  ZEND_ARG_INFO(0, flags)
  ZEND_ARG_INFO(0, options)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_tideways_disable, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_tideways_transaction_name, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_tideways_prepend_overwritten, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_tideways_fatal_backtrace, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_tideways_last_detected_exception, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_tideways_last_fatal_error, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_tideways_sql_minify, 0, 0, 0)
	ZEND_ARG_INFO(0, sql)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_tideways_span_create, 0, 0, 0)
	ZEND_ARG_INFO(0, category)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_tideways_get_spans, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_tideways_span_timer_start, 0, 0, 0)
	ZEND_ARG_INFO(0, span)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_tideways_span_timer_stop, 0, 0, 0)
	ZEND_ARG_INFO(0, span)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_tideways_span_annotate, 0, 0, 0)
	ZEND_ARG_INFO(0, span)
	ZEND_ARG_INFO(0, annotations)
ZEND_END_ARG_INFO()

/* }}} */

/**
 * *********************
 * FUNCTION PROTOTYPES
 * *********************
 */
int restore_cpu_affinity(cpu_set_t * prev_mask);
int bind_to_cpu(uint32 cpu_id);

/**
 * *********************
 * PHP EXTENSION GLOBALS
 * *********************
 */
/* List of functions implemented/exposed by Tideways */
zend_function_entry tideways_functions[] = {
	PHP_FE(tideways_enable, arginfo_tideways_enable)
	PHP_FE(tideways_disable, arginfo_tideways_disable)
	PHP_FE(tideways_transaction_name, arginfo_tideways_transaction_name)
	PHP_FE(tideways_prepend_overwritten, arginfo_tideways_prepend_overwritten)
	PHP_FE(tideways_fatal_backtrace, arginfo_tideways_fatal_backtrace)
	PHP_FE(tideways_last_detected_exception, arginfo_tideways_last_detected_exception)
	PHP_FE(tideways_last_fatal_error, arginfo_tideways_last_fatal_error)
	PHP_FE(tideways_sql_minify, arginfo_tideways_sql_minify)
	PHP_FE(tideways_span_create, arginfo_tideways_span_create)
	PHP_FE(tideways_get_spans, arginfo_tideways_get_spans)
	PHP_FE(tideways_span_timer_start, arginfo_tideways_span_timer_start)
	PHP_FE(tideways_span_timer_stop, arginfo_tideways_span_timer_stop)
	PHP_FE(tideways_span_annotate, arginfo_tideways_span_annotate)
	{NULL, NULL, NULL}
};

/* Callback functions for the Tideways extension */
zend_module_entry tideways_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"tideways",                        /* Name of the extension */
	tideways_functions,                /* List of functions exposed */
	PHP_MINIT(tideways),               /* Module init callback */
	PHP_MSHUTDOWN(tideways),           /* Module shutdown callback */
	PHP_RINIT(tideways),               /* Request init callback */
	PHP_RSHUTDOWN(tideways),           /* Request shutdown callback */
	PHP_MINFO(tideways),               /* Module info callback */
#if ZEND_MODULE_API_NO >= 20010901
	TIDEWAYS_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};

PHP_INI_BEGIN()

/**
 * INI-Settings are always used by the extension, but by the PHP library.
 */
PHP_INI_ENTRY("tideways.connection", "unix:///var/run/tideways/tidewaysd.sock", PHP_INI_ALL, NULL)
PHP_INI_ENTRY("tideways.udp_connection", "127.0.0.1:8135", PHP_INI_ALL, NULL)
PHP_INI_ENTRY("tideways.auto_start", "1", PHP_INI_ALL, NULL)
PHP_INI_ENTRY("tideways.api_key", "", PHP_INI_ALL, NULL)
PHP_INI_ENTRY("tideways.framework", "", PHP_INI_ALL, NULL)
PHP_INI_ENTRY("tideways.sample_rate", "10", PHP_INI_ALL, NULL)
PHP_INI_ENTRY("tideways.auto_prepend_library", "1", PHP_INI_ALL, NULL)

PHP_INI_END()

/* Init module */
ZEND_GET_MODULE(tideways)


/**
 * **********************************
 * PHP EXTENSION FUNCTION DEFINITIONS
 * **********************************
 */

/**
 * Start Tideways profiling in hierarchical mode.
 *
 * @param  long $flags  flags for hierarchical mode
 * @return void
 * @author kannan
 */
PHP_FUNCTION(tideways_enable)
{
	long  tideways_flags = 0;       /* Tideways flags */
	zval *optional_array = NULL;         /* optional array arg: for future use */

	if (hp_globals.enabled) {
		return;
	}

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
				"|lz", &tideways_flags, &optional_array) == FAILURE) {
		return;
	}

	hp_parse_options_from_arg(optional_array);

	hp_begin(tideways_flags TSRMLS_CC);
}

/**
 * Stops Tideways from profiling  and returns the profile info.
 *
 * @param  void
 * @return array  hash-array of Tideways's profile info
 * @author cjiang
 */
PHP_FUNCTION(tideways_disable)
{
	zval *tmp, *value;
	void *data;

	if (!hp_globals.enabled) {
		return;
	}

	hp_stop(TSRMLS_C);

	RETURN_ZVAL(hp_globals.stats_count, 1, 0);
}

PHP_FUNCTION(tideways_transaction_name)
{
	if (hp_globals.transaction_name) {
		zval *ret = hp_string_to_zval(hp_globals.transaction_name);
		RETURN_ZVAL(ret, 1, 1);
	}
}

PHP_FUNCTION(tideways_prepend_overwritten)
{
	RETURN_BOOL(hp_globals.prepend_overwritten);
}

PHP_FUNCTION(tideways_fatal_backtrace)
{
	if (hp_globals.backtrace != NULL) {
		RETURN_ZVAL(hp_globals.backtrace, 1, 1);
	}
}

PHP_FUNCTION(tideways_last_detected_exception)
{
	if (hp_globals.exception != NULL) {
		RETURN_ZVAL(hp_globals.exception, 1, 0);
	}
}

PHP_FUNCTION(tideways_last_fatal_error)
{
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	if (PG(last_error_message)) {
		array_init(return_value);
		add_assoc_long_ex(return_value, "type", sizeof("type"), PG(last_error_type));
		add_assoc_string_ex(return_value, "message", sizeof("message"), PG(last_error_message), 1);
		add_assoc_string_ex(return_value, "file", sizeof("file"), PG(last_error_file)?PG(last_error_file):"-", 1 );
		add_assoc_long_ex(return_value, "line", sizeof("line"), PG(last_error_lineno));
	}
}

long tw_span_create(char *category, size_t category_len)
{
	zval *span, *starts, *stops, *annotations;
	int idx;

	idx = zend_hash_num_elements(Z_ARRVAL_P(hp_globals.spans));

	MAKE_STD_ZVAL(span);
	MAKE_STD_ZVAL(starts);
	MAKE_STD_ZVAL(stops);
	MAKE_STD_ZVAL(annotations);

	array_init(span);
	array_init(starts);
	array_init(stops);
	array_init(annotations);

	add_assoc_stringl(span, "n", category, category_len, 1);
	add_assoc_zval(span, "b", starts);
	add_assoc_zval(span, "e", stops);
	add_assoc_zval(span, "a", annotations);

	zend_hash_index_update(Z_ARRVAL_P(hp_globals.spans), idx, &span, sizeof(zval*), NULL);

	return idx;
}

void tw_span_timer_start(long spanId)
{
	zval **span, **starts;
	double wt;

	if (zend_hash_index_find(Z_ARRVAL_P(hp_globals.spans), spanId, (void **) &span) == FAILURE) {
		return;
	}

	if (zend_hash_find(Z_ARRVAL_PP(span), "b", sizeof("b"), (void **) &starts) == FAILURE) {
		return;
	}

	wt = get_us_from_tsc(cycle_timer() - hp_globals.start_time, hp_globals.cpu_frequencies[hp_globals.cur_cpu_id]);
	add_next_index_long(*starts, wt);
}

void tw_span_record_duration(long spanId, double start, double end)
{
	zval **span, **timer;

	if (zend_hash_index_find(Z_ARRVAL_P(hp_globals.spans), spanId, (void **) &span) == FAILURE) {
		return;
	}

	if (zend_hash_find(Z_ARRVAL_PP(span), "e", sizeof("e"), (void **) &timer) == FAILURE) {
		return;
	}

	add_next_index_long(*timer, end);

	if (zend_hash_find(Z_ARRVAL_PP(span), "b", sizeof("b"), (void **) &timer) == FAILURE) {
		return;
	}

	add_next_index_long(*timer, start);
}

void tw_span_timer_stop(long spanId)
{
	zval **span, **stops;
	double wt;

	if (zend_hash_index_find(Z_ARRVAL_P(hp_globals.spans), spanId, (void **) &span) == FAILURE) {
		return;
	}

	if (zend_hash_find(Z_ARRVAL_PP(span), "e", sizeof("e"), (void **) &stops) == FAILURE) {
		return;
	}

	wt = get_us_from_tsc(cycle_timer() - hp_globals.start_time, hp_globals.cpu_frequencies[hp_globals.cur_cpu_id]);
	add_next_index_long(*stops, wt);
}

void tw_span_annotate(long spanId, zval *annotations)
{
	zval **span, **span_annotations;

	if (zend_hash_index_find(Z_ARRVAL_P(hp_globals.spans), spanId, (void **) &span) == FAILURE) {
		return;
	}

	if (zend_hash_find(Z_ARRVAL_PP(span), "a", sizeof("a"), (void **) &span_annotations) == FAILURE) {
		return;
	}

	zend_hash_merge(Z_ARRVAL_PP(span_annotations), Z_ARRVAL_P(annotations), (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *), 1);
}

void tw_span_annotate_string(long spanId, char *key, char *value, int copy)
{
	zval **span, **span_annotations, *zval;

	if (zend_hash_index_find(Z_ARRVAL_P(hp_globals.spans), spanId, (void **) &span) == FAILURE) {
		return;
	}

	if (zend_hash_find(Z_ARRVAL_PP(span), "a", sizeof("a"), (void **) &span_annotations) == FAILURE) {
		return;
	}

	add_assoc_string_ex(*span_annotations, key, strlen(key)+1, value, copy);
}

PHP_FUNCTION(tideways_span_create)
{
	char *category;
	size_t category_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &category, &category_len) == FAILURE) {
		return;
	}

	if (hp_globals.enabled == 0) {
		return;
	}

	RETURN_LONG(tw_span_create(category, category_len));
}

PHP_FUNCTION(tideways_get_spans)
{
	if (hp_globals.enabled) {
		RETURN_ZVAL(hp_globals.spans, 1, 0);
	}
}

PHP_FUNCTION(tideways_span_timer_start)
{
	long spanId;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &spanId) == FAILURE) {
		return;
	}

	if (hp_globals.enabled == 0) {
		return;
	}

	tw_span_timer_start(spanId);
}

PHP_FUNCTION(tideways_span_timer_stop)
{
	long spanId;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &spanId) == FAILURE) {
		return;
	}

	if (hp_globals.enabled == 0) {
		return;
	}

	tw_span_timer_stop(spanId);
}

PHP_FUNCTION(tideways_span_annotate)
{
	long spanId;
	zval *annotations;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "lz", &spanId, &annotations) == FAILURE) {
		return;
	}

	if (hp_globals.enabled == 0) {
		return;
	}

	tw_span_annotate(spanId, annotations);
}

PHP_FUNCTION(tideways_sql_minify)
{
	char *sql, *minified;
	int len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &sql, &len) == FAILURE) {
		return;
	}

	minified = hp_get_sql_summary(sql, len TSRMLS_DC);

	RETURN_STRING(minified, 0);
}

/**
 * Module init callback.
 *
 * @author cjiang
 */
PHP_MINIT_FUNCTION(tideways)
{
	int i;

	REGISTER_INI_ENTRIES();

	hp_register_constants(INIT_FUNC_ARGS_PASSTHRU);

	/* Get the number of available logical CPUs. */
	hp_globals.cpu_num = sysconf(_SC_NPROCESSORS_CONF);
	hp_globals.invariant_tsc = is_invariant_tsc();

	/* Get the cpu affinity mask. */
#ifndef __APPLE__
	if (GET_AFFINITY(0, sizeof(cpu_set_t), &hp_globals.prev_mask) < 0) {
		perror("getaffinity");
		return FAILURE;
	}
#else
	CPU_ZERO(&(hp_globals.prev_mask));
#endif

	/* Initialize cpu_frequencies and cur_cpu_id. */
	hp_globals.cpu_frequencies = NULL;
	hp_globals.cur_cpu_id = 0;

	hp_globals.stats_count = NULL;
	hp_globals.spans = NULL;

	/* no free hp_entry_t structures to start with */
	hp_globals.entry_free_list = NULL;

	for (i = 0; i < 256; i++) {
		hp_globals.func_hash_counters[i] = 0;
	}

	hp_transaction_function_clear();
	hp_exception_function_clear();

#if defined(DEBUG)
	/* To make it random number generator repeatable to ease testing. */
	srand(0);
#endif
	return SUCCESS;
}

/**
 * Module shutdown callback.
 */
PHP_MSHUTDOWN_FUNCTION(tideways)
{
	/* Make sure cpu_frequencies is free'ed. */
	clear_frequencies();

	/* free any remaining items in the free list */
	hp_free_the_free_list();

	UNREGISTER_INI_ENTRIES();

	return SUCCESS;
}

void tw_trace_callback_pgsql_execute(char *symbol, void **args, int args_len, zval *object, double start, double end TSRMLS_DC)
{
	long idx, *idx_ptr;
	zval *argument_element;
	char *summary;
	int i;

	for (i = 0; i < args_len; i++) {
		argument_element = *(args-(args_len-i));

		if (argument_element && Z_TYPE_P(argument_element) == IS_STRING && Z_STRLEN_P(argument_element) > 0) {
			// TODO: Introduce SQL statement cache to find the names here again.
			summary = Z_STRVAL_P(argument_element);

			if (zend_hash_find(hp_globals.span_cache, summary, strlen(summary)+1, (void **)&idx_ptr) == SUCCESS) {
				idx = *idx_ptr;
			} else {
				idx = tw_span_create("sql", 3);
				zend_hash_update(hp_globals.span_cache, summary, strlen(summary)+1, &idx, sizeof(long), NULL);
			}

			tw_span_record_duration(idx, start, end);
			tw_span_annotate_string(idx, "title", summary, 1);
			return;
		}
	}
}

void tw_trace_callback_pgsql_query(char *symbol, void **args, int args_len, zval *object, double start, double end TSRMLS_DC)
{
	long idx, *idx_ptr;
	zval *argument_element;
	char *summary;
	int i;

	for (i = 0; i < args_len; i++) {
		argument_element = *(args-(args_len-i));

		if (argument_element && Z_TYPE_P(argument_element) == IS_STRING) {
			summary = hp_get_sql_summary(Z_STRVAL_P(argument_element), Z_STRLEN_P(argument_element) TSRMLS_CC);

			if (zend_hash_find(hp_globals.span_cache, summary, strlen(summary)+1, (void **)&idx_ptr) == SUCCESS) {
				idx = *idx_ptr;
			} else {
				idx = tw_span_create("sql", 3);
				zend_hash_update(hp_globals.span_cache, summary, strlen(summary)+1, &idx, sizeof(long), NULL);
			}

			tw_span_record_duration(idx, start, end);
			tw_span_annotate_string(idx, "title", summary, 0);
			return;
		}
	}
}

void tw_trace_callback_smarty2_template(char *symbol, void **args, int args_len, zval *object, double start, double end TSRMLS_DC)
{
	long idx, *idx_ptr;
	zval *argument_element = *(args-args_len);

	if (argument_element && Z_TYPE_P(argument_element) == IS_STRING) {
		if (zend_hash_find(hp_globals.span_cache, Z_STRVAL_P(argument_element), Z_STRLEN_P(argument_element)+1, (void **)&idx_ptr) == SUCCESS) {
			idx = *idx_ptr;
		} else {
			idx = tw_span_create("view", 4);
			zend_hash_update(hp_globals.span_cache, Z_STRVAL_P(argument_element), Z_STRLEN_P(argument_element)+1, &idx, sizeof(long), NULL);
		}

		tw_span_record_duration(idx, start, end);
		tw_span_annotate_string(idx, "title", Z_STRVAL_P(argument_element), 1);
	}
}

void tw_trace_callback_smarty3_template(char *symbol, void **args, int args_len, zval *object, double start, double end TSRMLS_DC)
{
	long idx, *idx_ptr;
	zval *argument_element = *(args-args_len);
	zval *obj;
	zend_class_entry *smarty_ce;
	char *template;
	size_t template_len;

	if (argument_element && Z_TYPE_P(argument_element) == IS_STRING) {
		template = Z_STRVAL_P(argument_element);
	} else {
		smarty_ce = Z_OBJCE_P(object);

		argument_element = zend_read_property(smarty_ce, object, "template_resource", sizeof("template_resource") - 1, 1 TSRMLS_CC);
		template = Z_STRVAL_P(argument_element);
	}

	template_len = Z_STRLEN_P(argument_element);

	if (zend_hash_find(hp_globals.span_cache, template, template_len+1, (void **)&idx_ptr) == SUCCESS) {
		idx = *idx_ptr;
	} else {
		idx = tw_span_create("view", 4);
		zend_hash_update(hp_globals.span_cache, template, template_len+1, &idx, sizeof(long), NULL);
	}

	tw_span_record_duration(idx, start, end);
	tw_span_annotate_string(idx, "title", template, 1);
}

void tw_trace_callback_twig_template(char *symbol, void **args, int args_len, zval *object, double start, double end TSRMLS_DC)
{
	long idx, *idx_ptr;
	zval fname, *retval_ptr;

	if (object == NULL || Z_TYPE_P(object) != IS_OBJECT) {
		return;
	}

	ZVAL_STRING(&fname, "getTemplateName", 0);

	if (SUCCESS == call_user_function_ex(EG(function_table), &object, &fname, &retval_ptr, 0, NULL, 1, NULL TSRMLS_CC)) {
		if (zend_hash_find(hp_globals.span_cache, Z_STRVAL_P(retval_ptr), Z_STRLEN_P(retval_ptr)+1, (void **)&idx_ptr) == SUCCESS) {
			idx = *idx_ptr;
		} else {
			idx = tw_span_create("view", 4);
			zend_hash_update(hp_globals.span_cache, Z_STRVAL_P(retval_ptr), Z_STRLEN_P(retval_ptr)+1, &idx, sizeof(long), NULL);
		}

		tw_span_record_duration(idx, start, end);
		tw_span_annotate_string(idx, "title", Z_STRVAL_P(retval_ptr), 1);

		FREE_ZVAL(retval_ptr);
	}
}

void tw_trace_callback_event_dispatchers(char *symbol, void **args, int args_len, zval *object, double start, double end TSRMLS_DC)
{
	long idx, *idx_ptr;
	zval *argument_element = *(args-args_len);

	if (argument_element && Z_TYPE_P(argument_element) == IS_STRING) {
		if (zend_hash_find(hp_globals.span_cache, Z_STRVAL_P(argument_element), Z_STRLEN_P(argument_element)+1, (void **)&idx_ptr) == SUCCESS) {
			idx = *idx_ptr;
		} else {
			idx = tw_span_create("event", 5);
			zend_hash_update(hp_globals.span_cache, Z_STRVAL_P(argument_element), Z_STRLEN_P(argument_element)+1, &idx, sizeof(long), NULL);
		}

		tw_span_record_duration(idx, start, end);
		tw_span_annotate_string(idx, "title", Z_STRVAL_P(argument_element), 1);
	}
}

void tw_trace_callback_pdo_stmt_execute(char *symbol, void **args, int args_len, zval *object, double start, double end TSRMLS_DC)
{
	long idx, *idx_ptr;
	pdo_stmt_t *stmt = (pdo_stmt_t*)zend_object_store_get_object_by_handle(Z_OBJ_HANDLE_P(object) TSRMLS_CC);
	char *summary = hp_get_sql_summary(stmt->query_string, stmt->query_stringlen TSRMLS_CC);

	if (zend_hash_find(hp_globals.span_cache, summary, strlen(summary)+1, (void **)&idx_ptr) == SUCCESS) {
		idx = *idx_ptr;
	} else {
		idx = tw_span_create("sql", 3);
		zend_hash_update(hp_globals.span_cache, summary, strlen(summary)+1, &idx, sizeof(long), NULL);
	}

	tw_span_record_duration(idx, start, end);
	tw_span_annotate_string(idx, "title", summary, 0);
}

void tw_trace_callback_sql_functions(char *symbol, void **args, int args_len, zval *object, double start, double end TSRMLS_DC)
{
	zval *argument_element;
	char *summary;
	long idx, *idx_ptr;

	if (strcmp(symbol, "mysqli_query#") == 0) {
		argument_element = *(args-args_len+1);
	} else {
		argument_element = *(args-args_len);
	}

	if (Z_TYPE_P(argument_element) != IS_STRING) {
		return;
	}

	summary = hp_get_sql_summary(Z_STRVAL_P(argument_element), Z_STRLEN_P(argument_element) TSRMLS_CC);

	if (zend_hash_find(hp_globals.span_cache, summary, strlen(summary)+1, (void **)&idx_ptr) == SUCCESS) {
		idx = *idx_ptr;
	} else {
		idx = tw_span_create("sql", 3);
		zend_hash_update(hp_globals.span_cache, summary, strlen(summary)+1, &idx, sizeof(long), NULL);
	}

	tw_span_record_duration(idx, start, end);
	tw_span_annotate_string(idx, "title", summary, 0);
}

void tw_trace_callback_curl_exec(char *symbol, void **args, int args_len, zval *object, double start, double end TSRMLS_DC)
{
	zval *argument = *(args-args_len);
	zval **option;
	zval ***params_array;
	char *summary;
	long idx, *idx_ptr;
	zval fname, *retval_ptr, *opt;

	if (argument == NULL || Z_TYPE_P(argument) != IS_RESOURCE) {
		return;
	}

	ZVAL_STRING(&fname, "curl_getinfo", 0);

	params_array = (zval ***) emalloc(sizeof(zval **));
	params_array[0] = &argument;

	if (SUCCESS == call_user_function_ex(EG(function_table), NULL, &fname, &retval_ptr, 1, params_array, 1, NULL TSRMLS_CC)) {
		if (zend_hash_find(Z_ARRVAL_P(retval_ptr), "url", sizeof("url"), (void **)&option) == SUCCESS) {
			summary = hp_get_file_summary(Z_STRVAL_PP(option), Z_STRLEN_PP(option) TSRMLS_CC);

			if (zend_hash_find(hp_globals.span_cache, summary, strlen(summary)+1, (void **)&idx_ptr) == SUCCESS) {
				idx = *idx_ptr;
			} else {
				idx = tw_span_create("http", 4);
				zend_hash_update(hp_globals.span_cache, summary, strlen(summary)+1, &idx, sizeof(long), NULL);
			}

			tw_span_record_duration(idx, start, end);
			tw_span_annotate_string(idx, "title", summary, 0);
		}

		zval_dtor(retval_ptr);
		FREE_ZVAL(retval_ptr);
	}

	efree(params_array);
}

void tw_trace_callback_file_get_contents(char *symbol, void **args, int args_len, zval *object, double start, double end TSRMLS_DC)
{
	zval *argument = *(args-args_len);
	char *summary;
	long idx, *idx_ptr;

	if (Z_TYPE_P(argument) != IS_STRING) {
		return;
	}

	if (strncmp(Z_STRVAL_P(argument), "http", 4) != 0) {
		return;
	}

	summary = hp_get_file_summary(Z_STRVAL_P(argument), Z_STRLEN_P(argument) TSRMLS_CC);

	if (zend_hash_find(hp_globals.span_cache, summary, strlen(summary)+1, (void **)&idx_ptr) == SUCCESS) {
		idx = *idx_ptr;
	} else {
		idx = tw_span_create("http", 4);
		zend_hash_update(hp_globals.span_cache, summary, strlen(summary)+1, &idx, sizeof(long), NULL);
	}

	tw_span_record_duration(idx, start, end);
	tw_span_annotate_string(idx, "title", summary, 0);
}

/**
 * Request init callback.
 *
 * Check if Tideways.php exists in extension_dir and load it
 * in request init. This makes class \Tideways\Profiler available
 * for usage.
 */
PHP_RINIT_FUNCTION(tideways)
{
	char *extension_dir;
	char *profiler_file;
	int profiler_file_len;
	tw_trace_callback *cb;

	hp_globals.prepend_overwritten = 0;
	hp_globals.backtrace = NULL;
	hp_globals.exception = NULL;

	hp_globals.trace_callbacks = NULL;
	ALLOC_HASHTABLE(hp_globals.trace_callbacks);
	zend_hash_init(hp_globals.trace_callbacks, 32, NULL, NULL, 0);

	hp_globals.span_cache = NULL;
	ALLOC_HASHTABLE(hp_globals.span_cache);
	zend_hash_init(hp_globals.span_cache, 32, NULL, NULL, 0);

	cb = tw_trace_callback_file_get_contents;
	register_trace_callback("file_get_contents", cb);

	cb = tw_trace_callback_curl_exec;
	register_trace_callback("curl_exec", cb);

	cb = tw_trace_callback_sql_functions;
	register_trace_callback("PDO::exec", cb);
	register_trace_callback("PDO::query", cb);
	register_trace_callback("mysql_query", cb);
	register_trace_callback("mysqli_query", cb);
	register_trace_callback("mysqli::query", cb);

	cb = tw_trace_callback_pdo_stmt_execute;
	register_trace_callback("PDOStatement::execute", cb);

	cb = tw_trace_callback_pgsql_query;
	register_trace_callback("pg_query", cb);
	register_trace_callback("pg_query_params", cb);

	cb = tw_trace_callback_pgsql_execute;
	register_trace_callback("pg_execute", cb);

	cb = tw_trace_callback_event_dispatchers;
	register_trace_callback("Symfony\\Component\\EventDispatcher\\EventDispatcher::dispatch", cb);
	register_trace_callback("Doctrine\\Common\\EventManager::dispatchEvent", cb);
	register_trace_callback("Enlight_Event_EventManager::filter", cb);
	register_trace_callback("Enlight_Event_EventManager::notify", cb);
	register_trace_callback("Enlight_Event_EventManager::notifyUntil", cb);
	register_trace_callback("Zend\\EventManager\\EventManager::trigger", cb);
	register_trace_callback("do_action", cb);
	register_trace_callback("apply_filters", cb);
	register_trace_callback("drupal_alter", cb);
	register_trace_callback("Mage::dispatchEvent", cb);

	cb = tw_trace_callback_twig_template;
	register_trace_callback("Twig_Template::render", cb);
	register_trace_callback("Twig_Template::display", cb);

	cb = tw_trace_callback_smarty2_template;
	register_trace_callback("Smarty::fetch", cb);

	cb = tw_trace_callback_smarty3_template;
	register_trace_callback("Smarty_Internal_TemplateBase::fetch", cb);

	if (INI_INT("tideways.auto_prepend_library") == 0) {
		return SUCCESS;
	}

	extension_dir  = INI_STR("extension_dir");
	profiler_file_len = strlen(extension_dir) + strlen("Tideways.php") + 2;
	profiler_file = emalloc(profiler_file_len);
	snprintf(profiler_file, profiler_file_len, "%s/%s", extension_dir, "Tideways.php");

	if (VCWD_ACCESS(profiler_file, F_OK) == 0) {
		PG(auto_prepend_file) = profiler_file;
		hp_globals.prepend_overwritten = 1;
	} else {
		efree(profiler_file);
	}

	return SUCCESS;
}

/**
 * Request shutdown callback. Stop profiling and return.
 */
PHP_RSHUTDOWN_FUNCTION(tideways)
{
	hp_end(TSRMLS_C);

	// @todo move to hp_stop
	zend_hash_destroy(hp_globals.trace_callbacks);
	FREE_HASHTABLE(hp_globals.trace_callbacks);

	zend_hash_destroy(hp_globals.span_cache);
	FREE_HASHTABLE(hp_globals.span_cache);

	if (hp_globals.prepend_overwritten == 1) {
		efree(PG(auto_prepend_file));
	}
	PG(auto_prepend_file) = NULL;
	hp_globals.prepend_overwritten = 0;

	return SUCCESS;
}

/**
 * Module info callback. Returns the Tideways version.
 */
PHP_MINFO_FUNCTION(tideways)
{
	char buf[SCRATCH_BUF_LEN];
	char tmp[SCRATCH_BUF_LEN];
	int i;
	int len;
	int found = 0;

	php_info_print_table_start();
	php_info_print_table_header(2, "tideways", TIDEWAYS_VERSION);
	php_info_print_table_row(2, "TSC Invariant", is_invariant_tsc() ? "Yes" : "No");
	len = snprintf(buf, SCRATCH_BUF_LEN, "%d", hp_globals.cpu_num);
	buf[len] = 0;
	php_info_print_table_header(2, "CPU num", buf);

	if (hp_globals.cpu_frequencies) {
		/* Print available cpu frequencies here. */
		php_info_print_table_header(2, "CPU logical id", " Clock Rate (MHz) ");
		for (i = 0; i < hp_globals.cpu_num; ++i) {
			len = snprintf(buf, SCRATCH_BUF_LEN, " CPU %d ", i);
			buf[len] = 0;
			len = snprintf(tmp, SCRATCH_BUF_LEN, "%f", hp_globals.cpu_frequencies[i]);
			tmp[len] = 0;
			php_info_print_table_row(2, buf, tmp);
		}
	}
	php_info_print_table_row(2, "Connection (tideways.connection)", INI_STR("tideways.connection"));
	php_info_print_table_row(2, "UDP Connection (tideways.udp_connection)", INI_STR("tideways.udp_connection"));
	php_info_print_table_row(2, "Default API Key (tideways.api_key)", INI_STR("tideways.api_key"));
	php_info_print_table_row(2, "Default Sample-Rate (tideways.sample_rate)", INI_STR("tideways.sample_rate"));
	php_info_print_table_row(2, "Framework Detection (tideways.framework)", INI_STR("tideways.framework"));
	php_info_print_table_row(2, "Automatically Start (tideways.auto_start)", INI_INT("tideways.auto_start") ? "Yes": "No");
	php_info_print_table_row(2, "Load PHP Library (tideways.auto_prepend_library)", INI_INT("tideways.auto_prepend_library") ? "Yes": "No");

	php_info_print_table_end();
}


/**
 * ***************************************************
 * COMMON HELPER FUNCTION DEFINITIONS AND LOCAL MACROS
 * ***************************************************
 */

static void hp_register_constants(INIT_FUNC_ARGS)
{
	REGISTER_LONG_CONSTANT("TIDEWAYS_FLAGS_NO_BUILTINS",
			TIDEWAYS_FLAGS_NO_BUILTINS,
			CONST_CS | CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("TIDEWAYS_FLAGS_CPU",
			TIDEWAYS_FLAGS_CPU,
			CONST_CS | CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("TIDEWAYS_FLAGS_MEMORY",
			TIDEWAYS_FLAGS_MEMORY,
			CONST_CS | CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("TIDEWAYS_FLAGS_NO_USERLAND",
			TIDEWAYS_FLAGS_NO_USERLAND,
			CONST_CS | CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("TIDEWAYS_FLAGS_NO_COMPILE",
			TIDEWAYS_FLAGS_NO_COMPILE,
			CONST_CS | CONST_PERSISTENT);
}

/**
 * A hash function to calculate a 8-bit hash code for a function name.
 * This is based on a small modification to 'zend_inline_hash_func' by summing
 * up all bytes of the ulong returned by 'zend_inline_hash_func'.
 *
 * @param str, char *, string to be calculated hash code for.
 *
 * @author cjiang
 */
static inline uint8 hp_inline_hash(char * arKey)
{
	size_t nKeyLength = strlen(arKey);
	register uint8 hash = 0;

	/* variant with the hash unrolled eight times */
	for (; nKeyLength >= 8; nKeyLength -= 8) {
		hash = ((hash << 5) + hash) + *arKey++;
		hash = ((hash << 5) + hash) + *arKey++;
		hash = ((hash << 5) + hash) + *arKey++;
		hash = ((hash << 5) + hash) + *arKey++;
		hash = ((hash << 5) + hash) + *arKey++;
		hash = ((hash << 5) + hash) + *arKey++;
		hash = ((hash << 5) + hash) + *arKey++;
		hash = ((hash << 5) + hash) + *arKey++;
	}
	switch (nKeyLength) {
		case 7: hash = ((hash << 5) + hash) + *arKey++; /* fallthrough... */
		case 6: hash = ((hash << 5) + hash) + *arKey++; /* fallthrough... */
		case 5: hash = ((hash << 5) + hash) + *arKey++; /* fallthrough... */
		case 4: hash = ((hash << 5) + hash) + *arKey++; /* fallthrough... */
		case 3: hash = ((hash << 5) + hash) + *arKey++; /* fallthrough... */
		case 2: hash = ((hash << 5) + hash) + *arKey++; /* fallthrough... */
		case 1: hash = ((hash << 5) + hash) + *arKey++; break;
		case 0: break;
EMPTY_SWITCH_DEFAULT_CASE()
	}
	return hash;
}

/**
 * Parse the list of ignored functions from the zval argument.
 *
 * @author mpal
 */
static void hp_parse_options_from_arg(zval *args)
{
	hp_clean_profiler_options_state();

	if (args == NULL) {
		return;
	}

	zval  *zresult = NULL;

	zresult = hp_zval_at_key("ignored_functions", args);

	if (zresult == NULL) {
		zresult = hp_zval_at_key("functions", args);
		if (zresult != NULL) {
			hp_globals.filtered_type = 2;
		}
	} else {
		hp_globals.filtered_type = 1;
	}

	hp_globals.filtered_functions = hp_function_map_create(hp_strings_in_zval(zresult));

	zresult = hp_zval_at_key("transaction_function", args);

	if (zresult != NULL) {
		hp_globals.transaction_function = hp_zval_to_string(zresult);
	}

	zresult = hp_zval_at_key("exception_function", args);

	if (zresult != NULL) {
		hp_globals.exception_function = hp_zval_to_string(zresult);
	}
}

static void hp_exception_function_clear() {
	if (hp_globals.exception_function != NULL) {
		hp_string_clean(hp_globals.exception_function);
		efree(hp_globals.exception_function);
		hp_globals.exception_function = NULL;
	}

	if (hp_globals.exception != NULL) {
		zval_ptr_dtor(&hp_globals.exception);
	}
}

static void hp_transaction_function_clear() {
	if (hp_globals.transaction_function) {
		hp_string_clean(hp_globals.transaction_function);
		efree(hp_globals.transaction_function);
		hp_globals.transaction_function = NULL;
	}
}

static inline hp_function_map *hp_function_map_create(char **names)
{
	if (names == NULL) {
		return NULL;
	}

	hp_function_map *map;

	map = emalloc(sizeof(hp_function_map));
	map->names = names;

	memset(map->filter, 0, TIDEWAYS_FILTERED_FUNCTION_SIZE);

	int i = 0;
	for(; names[i] != NULL; i++) {
		char *str  = names[i];
		uint8 hash = hp_inline_hash(str);
		int   idx  = INDEX_2_BYTE(hash);
		map->filter[idx] |= INDEX_2_BIT(hash);
	}

	return map;
}

static inline void hp_function_map_clear(hp_function_map *map) {
	if (map == NULL) {
		return;
	}

	hp_array_del(map->names);
	map->names = NULL;

	memset(map->filter, 0, TIDEWAYS_FILTERED_FUNCTION_SIZE);
	efree(map);
}

static inline int hp_function_map_exists(hp_function_map *map, uint8 hash_code, char *curr_func)
{
	if (hp_function_map_filter_collision(map, hash_code)) {
		int i = 0;
		for (; map->names[i] != NULL; i++) {
			char *name = map->names[i];
			if (strcmp(curr_func, name) == 0) {
				return 1;
			}
		}
	}

	return 0;
}


static inline int hp_function_map_filter_collision(hp_function_map *map, uint8 hash)
{
	uint8 mask = INDEX_2_BIT(hash);
	return map->filter[INDEX_2_BYTE(hash)] & mask;
}


/**
 * Initialize profiler state
 *
 * @author kannan, veeve
 */
void hp_init_profiler_state(TSRMLS_D)
{
	/* Setup globals */
	if (!hp_globals.ever_enabled) {
		hp_globals.ever_enabled  = 1;
		hp_globals.entries = NULL;
	}

	/* Init stats_count */
	if (hp_globals.stats_count) {
		zval_dtor(hp_globals.stats_count);
		FREE_ZVAL(hp_globals.stats_count);
	}
	MAKE_STD_ZVAL(hp_globals.stats_count);
	array_init(hp_globals.stats_count);

	if (hp_globals.spans) {
		zval_dtor(hp_globals.spans);
		FREE_ZVAL(hp_globals.spans);
	}
	MAKE_STD_ZVAL(hp_globals.spans);
	array_init(hp_globals.spans);

	hp_globals.start_time = cycle_timer();

	/* NOTE(cjiang): some fields such as cpu_frequencies take relatively longer
	 * to initialize, (5 milisecond per logical cpu right now), therefore we
	 * calculate them lazily. */
	if (hp_globals.cpu_frequencies == NULL) {
		get_all_cpu_frequencies();
		restore_cpu_affinity(&hp_globals.prev_mask);
	}

	/* bind to a random cpu so that we can use rdtsc instruction. */
	bind_to_cpu((int) (rand() % hp_globals.cpu_num));

	/* Set up filter of functions which may be ignored during profiling */
	hp_transaction_name_clear();
}

/**
 * Cleanup profiler state
 *
 * @author kannan, veeve
 */
void hp_clean_profiler_state(TSRMLS_D)
{
	/* Clear globals */
	if (hp_globals.stats_count) {
		zval_dtor(hp_globals.stats_count);
		FREE_ZVAL(hp_globals.stats_count);
		hp_globals.stats_count = NULL;
	}
	if (hp_globals.spans) {
		zval_dtor(hp_globals.spans);
		FREE_ZVAL(hp_globals.spans);
		hp_globals.spans = NULL;
	}

	hp_globals.entries = NULL;
	hp_globals.ever_enabled = 0;

	hp_clean_profiler_options_state();

	hp_function_map_clear(hp_globals.filtered_functions);
	hp_globals.filtered_functions = NULL;
}

static void hp_transaction_name_clear()
{
	if (hp_globals.transaction_name) {
		hp_string_clean(hp_globals.transaction_name);
		efree(hp_globals.transaction_name);
		hp_globals.transaction_name = NULL;
	}
}

static void hp_clean_profiler_options_state()
{
	hp_function_map_clear(hp_globals.filtered_functions);
	hp_globals.filtered_functions = NULL;

	hp_exception_function_clear();
	hp_transaction_function_clear();
	hp_transaction_name_clear();
}

/*
 * Start profiling - called just before calling the actual function
 * NOTE:  PLEASE MAKE SURE TSRMLS_CC IS AVAILABLE IN THE CONTEXT
 *        OF THE FUNCTION WHERE THIS MACRO IS CALLED.
 *        TSRMLS_CC CAN BE MADE AVAILABLE VIA TSRMLS_DC IN THE
 *        CALLING FUNCTION OR BY CALLING TSRMLS_FETCH()
 *        TSRMLS_FETCH() IS RELATIVELY EXPENSIVE.
 */
#define BEGIN_PROFILING(entries, symbol, profile_curr, execute_data)			\
	do {																		\
		/* Use a hash code to filter most of the string comparisons. */			\
		uint8 hash_code  = hp_inline_hash(symbol);								\
		profile_curr = !hp_filter_entry(hash_code, symbol);						\
		if (profile_curr) {														\
			hp_entry_t *cur_entry = hp_fast_alloc_hprof_entry();				\
			(cur_entry)->hash_code = hash_code;									\
			(cur_entry)->name_hprof = symbol;									\
			(cur_entry)->prev_hprof = (*(entries));								\
			hp_mode_hier_beginfn_cb((entries), (cur_entry) TSRMLS_CC);   \
			/* Update entries linked list */									\
			(*(entries)) = (cur_entry);											\
		}																		\
	} while (0)

/*
 * Stop profiling - called just after calling the actual function
 * NOTE:  PLEASE MAKE SURE TSRMLS_CC IS AVAILABLE IN THE CONTEXT
 *        OF THE FUNCTION WHERE THIS MACRO IS CALLED.
 *        TSRMLS_CC CAN BE MADE AVAILABLE VIA TSRMLS_DC IN THE
 *        CALLING FUNCTION OR BY CALLING TSRMLS_FETCH()
 *        TSRMLS_FETCH() IS RELATIVELY EXPENSIVE.
 */
#define END_PROFILING(entries, profile_curr, data)							\
	do {																	\
		if (profile_curr) {													\
			hp_entry_t *cur_entry;											\
			hp_mode_hier_endfn_cb((entries), data TSRMLS_CC);				\
			cur_entry = (*(entries));										\
			/* Free top entry and update entries linked list */				\
			(*(entries)) = (*(entries))->prev_hprof;						\
			hp_fast_free_hprof_entry(cur_entry);							\
		}																	\
	} while (0)

/**
 * Returns formatted function name
 *
 * @param  entry        hp_entry
 * @param  result_buf   ptr to result buf
 * @param  result_len   max size of result buf
 * @return total size of the function name returned in result_buf
 * @author veeve
 */
size_t hp_get_entry_name(hp_entry_t  *entry, char *result_buf, size_t result_len)
{
	/* Validate result_len */
	if (result_len <= 1) {
		/* Insufficient result_bug. Bail! */
		return 0;
	}

	/* Add '@recurse_level' if required */
	/* NOTE:  Dont use snprintf's return val as it is compiler dependent */
	if (entry->rlvl_hprof) {
		snprintf(
			result_buf,
			result_len,
			"%s@%d",
			entry->name_hprof,
			entry->rlvl_hprof
		);
	} else {
		strncat(
			result_buf,
			entry->name_hprof,
			result_len
		);
	}

	/* Force null-termination at MAX */
	result_buf[result_len - 1] = 0;

	return strlen(result_buf);
}

/**
 * Check if this entry should be filtered (positive or negative), first with a
 * conservative Bloomish filter then with an exact check against the function
 * names.
 *
 * @author mpal
 */
static inline int hp_filter_entry(uint8 hash_code, char *curr_func)
{
	int exists;

	/* First check if ignoring functions is enabled */
	if (hp_globals.filtered_functions == NULL || hp_globals.filtered_type == 0) {
		return 0;
	}

	exists = hp_function_map_exists(hp_globals.filtered_functions, hash_code, curr_func);

	if (hp_globals.filtered_type == 2) {
		// always include main() in profiling result.
		return (strcmp(curr_func, ROOT_SYMBOL) == 0)
			? 0
			: abs(1 - exists);
	}

	return exists;
}

/**
 * Build a caller qualified name for a callee.
 *
 * For example, if A() is caller for B(), then it returns "A==>B".
 * Recursive invokations are denoted with @<n> where n is the recursion
 * depth.
 *
 * For example, "foo==>foo@1", and "foo@2==>foo@3" are examples of direct
 * recursion. And  "bar==>foo@1" is an example of an indirect recursive
 * call to foo (implying the foo() is on the call stack some levels
 * above).
 *
 * @author kannan, veeve
 */
size_t hp_get_function_stack(hp_entry_t *entry, int level, char *result_buf, size_t result_len)
{
	size_t         len = 0;

	if (!entry->prev_hprof || (level <= 1)) {
		return hp_get_entry_name(entry, result_buf, result_len);
	}

	len = hp_get_function_stack(entry->prev_hprof, level - 1, result_buf, result_len);

	/* Append the delimiter */
# define    HP_STACK_DELIM        "==>"
# define    HP_STACK_DELIM_LEN    (sizeof(HP_STACK_DELIM) - 1)

	if (result_len < (len + HP_STACK_DELIM_LEN)) {
		return len;
	}

	if (len) {
		strncat(result_buf + len, HP_STACK_DELIM, result_len - len);
		len += HP_STACK_DELIM_LEN;
	}

# undef     HP_STACK_DELIM_LEN
# undef     HP_STACK_DELIM

	return len + hp_get_entry_name(entry, result_buf + len, result_len - len);
}

/**
 * Takes an input of the form /a/b/c/d/foo.php and returns
 * a pointer to one-level directory and basefile name
 * (d/foo.php) in the same string.
 */
static const char *hp_get_base_filename(const char *filename)
{
	const char *ptr;
	int   found = 0;

	if (!filename)
		return "";

	/* reverse search for "/" and return a ptr to the next char */
	for (ptr = filename + strlen(filename) - 1; ptr >= filename; ptr--) {
		if (*ptr == '/') {
			found++;
		}
		if (found == 2) {
			return ptr + 1;
		}
	}

	/* no "/" char found, so return the whole string */
	return filename;
}

/**
 * Extract a summary of the executed SQL from a query string.
 */
static char *hp_get_sql_summary(char *sql, int len TSRMLS_DC)
{
	zval *parts, **data;
	HashTable *arrayParts;
	pcre_cache_entry	*pce;			/* Compiled regular expression */
	HashPosition pointer;
	int array_count, result_len, found, found_select;
	char *result, *token;

	found = 0;
	found_select = 0;
	result = "";
	MAKE_STD_ZVAL(parts);

	if ((pce = pcre_get_compiled_regex_cache("(([\\s]+))", 8 TSRMLS_CC)) == NULL) {
		return "";
	}

	php_pcre_split_impl(pce, sql, len, parts, -1, 0 TSRMLS_CC);

	arrayParts = Z_ARRVAL_P(parts);

	result_len = TIDEWAYS_MAX_ARGUMENT_LEN;
	result = emalloc(result_len);

	for(zend_hash_internal_pointer_reset_ex(arrayParts, &pointer);
			zend_hash_get_current_data_ex(arrayParts, (void**) &data, &pointer) == SUCCESS;
			zend_hash_move_forward_ex(arrayParts, &pointer)) {

		char *key;
		int key_len;
		long index;

		zend_hash_get_current_key_ex(arrayParts, &key, &key_len, &index, 0, &pointer);

		token = Z_STRVAL_PP(data);
		php_strtolower(token, Z_STRLEN_PP(data));

		if ((strcmp(token, "insert") == 0 || strcmp(token, "delete") == 0) &&
				zend_hash_index_exists(arrayParts, index+2)) {
			snprintf(result, result_len, "%s", token);

			zend_hash_index_find(arrayParts, index+2, (void**) &data);
			snprintf(result, result_len, "%s %s", result, Z_STRVAL_PP(data));
			found = 1;

			break;
		} else if (strcmp(token, "update") == 0 && zend_hash_index_exists(arrayParts, index+1)) {
			snprintf(result, result_len, "%s", token);

			zend_hash_index_find(arrayParts, index+1, (void**) &data);
			snprintf(result, result_len, "%s %s", result, Z_STRVAL_PP(data));
			found = 1;

			break;
		} else if (strcmp(token, "select") == 0) {
			snprintf(result, result_len, "%s", token);
			found_select = 1;
		} else if (found_select == 1 && strcmp(token, "from") == 0) {
			zend_hash_index_find(arrayParts, index+1, (void**) &data);
			snprintf(result, result_len, "%s %s", result, Z_STRVAL_PP(data));
			found = 1;

			break;
		}
	}

	zval_ptr_dtor(&parts);

	if (found == 0) {
		snprintf(result, result_len, "%s", "other");
	}

	return result;
}

static char *hp_get_file_summary(char *filename, int filename_len TSRMLS_DC)
{
	php_url *url;
	char *ret;
	int len;

	len = TIDEWAYS_MAX_ARGUMENT_LEN;
	ret = emalloc(len);
	snprintf(ret, len, "");

	url = php_url_parse_ex(filename, filename_len);

	if (url->scheme) {
		snprintf(ret, len, "%s%s://", ret, url->scheme);
	} else {
		php_url_free(url);
		return ret;
	}

	if (url->host) {
		snprintf(ret, len, "%s%s", ret, url->host);
	}

	if (url->port) {
		snprintf(ret, len, "%s:%d", ret, url->port);
	}

	if (url->path) {
		snprintf(ret, len, "%s%s", ret, url->path);
	}

	php_url_free(url);

	return ret;
}

static inline void **hp_get_execute_arguments(zend_execute_data *data)
{
	void **p;

	p = data->function_state.arguments;

#if PHP_VERSION_ID >= 50500
	/*
	 * With PHP 5.5 zend_execute cannot be overwritten by extensions anymore.
	 * instead zend_execute_ex has to be used. That however does not have
	 * function_state.arguments populated for non-internal functions.
	 * As per UPGRADING.INTERNALS we are accessing prev_execute_data which
	 * has this information (for whatever reasons).
	 */
	if (p == NULL) {
		p = (*data).prev_execute_data->function_state.arguments;
	}
#endif

	return p;
}

static char *hp_concat_char(const char *s1, size_t len1, const char *s2, size_t len2, const char *seperator, size_t sep_len)
{
    char *result = emalloc(len1+len2+sep_len+1);

    strcpy(result, s1);
	strcat(result, seperator);
    strcat(result, s2);

    return result;
}

static void hp_detect_exception(char *func_name, zend_execute_data *data TSRMLS_DC)
{
	void **p = hp_get_execute_arguments(data);
	int arg_count = (int)(zend_uintptr_t) *p;
	zval *argument_element;
	int i;
	zend_class_entry *default_ce, *exception_ce;

	default_ce = zend_exception_get_default(TSRMLS_C);

	for (i=0; i < arg_count; i++) {
		argument_element = *(p-(arg_count-i));

		if (Z_TYPE_P(argument_element) == IS_OBJECT) {
			exception_ce = zend_get_class_entry(argument_element TSRMLS_CC);

			if (instanceof_function(exception_ce, default_ce TSRMLS_CC) == 1) {
				Z_ADDREF_P(argument_element);
				hp_globals.exception = argument_element;
				return;
			}
		}
	}
}

static void hp_detect_transaction_name(char *ret, zend_execute_data *data TSRMLS_DC)
{
	if (!hp_globals.transaction_function ||
		hp_globals.transaction_name ||
		strcmp(ret, hp_globals.transaction_function->value) != 0) {
		return;
	}

	void **p = hp_get_execute_arguments(data);
	int arg_count = (int)(zend_uintptr_t) *p;
	zval *argument_element;

	if (strcmp(ret, "Zend_Controller_Action::dispatch") == 0 ||
			   strcmp(ret, "Enlight_Controller_Action::dispatch") == 0 ||
			   strcmp(ret, "Mage_Core_Controller_Varien_Action::dispatch") == 0) {
		zval *obj = data->object;
		argument_element = *(p-arg_count);
		const char *class_name;
		zend_uint class_name_len;
		const char *free_class_name = NULL;

		if (!zend_get_object_classname(obj, &class_name, &class_name_len TSRMLS_CC)) {
			free_class_name = class_name;
		}

		if (Z_TYPE_P(argument_element) == IS_STRING) {
			int len = class_name_len + Z_STRLEN_P(argument_element) + 3;
			char *ret = NULL;
			ret = (char*)emalloc(len);
			snprintf(ret, len, "%s::%s", class_name, Z_STRVAL_P(argument_element));

			hp_globals.transaction_name = hp_create_string(ret, len);
			efree(ret);
		}

		if (free_class_name) {
			efree((char*)free_class_name);
		}
	} else {
		argument_element = *(p-arg_count);

		if (Z_TYPE_P(argument_element) == IS_STRING) {
			hp_globals.transaction_name = hp_zval_to_string(argument_element);
		}
	}

	hp_transaction_function_clear();
}

/**
 * Get the name of the current function. The name is qualified with
 * the class name if the function is in a class.
 *
 * @author kannan, hzhao
 */
static char *hp_get_function_name(zend_execute_data *data TSRMLS_DC)
{
	const char        *func = NULL;
	const char        *cls = NULL;
	char              *ret = NULL;
	int                len;
	zend_function      *curr_func;

	if (data) {
		/* shared meta data for function on the call stack */
		curr_func = data->function_state.function;

		/* extract function name from the meta info */
		func = curr_func->common.function_name;

		if (func) {
			/* previously, the order of the tests in the "if" below was
			 * flipped, leading to incorrect function names in profiler
			 * reports. When a method in a super-type is invoked the
			 * profiler should qualify the function name with the super-type
			 * class name (not the class name based on the run-time type
			 * of the object.
			 */
			if (curr_func->common.scope) {
				cls = curr_func->common.scope->name;
			} else if (data->object) {
				cls = Z_OBJCE(*data->object)->name;
			}

			if (cls) {
				char* sep = "::";
				ret = hp_concat_char(cls, strlen(cls), func, strlen(func), sep, 2);
			} else {
				ret = estrdup(func);
			}
		} else {
			long     curr_op;
			int      add_filename = 1;

			/* we are dealing with a special directive/function like
			 * include, eval, etc.
			 */
#if ZEND_EXTENSION_API_NO >= 220100525
			curr_op = data->opline->extended_value;
#else
			curr_op = data->opline->op2.u.constant.value.lval;
#endif

			/* For some operations, we'll add the filename as part of the function
			 * name to make the reports more useful. So rather than just "include"
			 * you'll see something like "run_init::foo.php" in your reports.
			 */
			if (curr_op == ZEND_EVAL){
				return NULL;
			} else {
				const char *filename;
				int   len;
				filename = hp_get_base_filename((curr_func->op_array).filename);
				len      = strlen("run_init") + strlen(filename) + 3;
				ret      = (char *)emalloc(len);
				snprintf(ret, len, "run_init::%s", filename);
			}
		}
	}
	return ret;
}

/**
 * Free any items in the free list.
 */
static void hp_free_the_free_list()
{
	hp_entry_t *p = hp_globals.entry_free_list;
	hp_entry_t *cur;

	while (p) {
		cur = p;
		p = p->prev_hprof;
		free(cur);
	}
}

/**
 * Fast allocate a hp_entry_t structure. Picks one from the
 * free list if available, else does an actual allocate.
 *
 * Doesn't bother initializing allocated memory.
 *
 * @author kannan
 */
static hp_entry_t *hp_fast_alloc_hprof_entry()
{
	hp_entry_t *p;

	p = hp_globals.entry_free_list;

	if (p) {
		hp_globals.entry_free_list = p->prev_hprof;
		return p;
	} else {
		return (hp_entry_t *)malloc(sizeof(hp_entry_t));
	}
}

/**
 * Fast free a hp_entry_t structure. Simply returns back
 * the hp_entry_t to a free list and doesn't actually
 * perform the free.
 *
 * @author kannan
 */
static void hp_fast_free_hprof_entry(hp_entry_t *p)
{
	/* we use/overload the prev_hprof field in the structure to link entries in
	 * the free list. */
	p->prev_hprof = hp_globals.entry_free_list;
	hp_globals.entry_free_list = p;
}

/**
 * Increment the count of the given stat with the given count
 * If the stat was not set before, inits the stat to the given count
 *
 * @param  zval *counts   Zend hash table pointer
 * @param  char *name     Name of the stat
 * @param  long  count    Value of the stat to incr by
 * @return void
 * @author kannan
 */
void hp_inc_count(zval *counts, char *name, long count TSRMLS_DC)
{
	HashTable *ht;
	void *data;

	if (!counts) return;
	ht = HASH_OF(counts);
	if (!ht) return;

	if (zend_hash_find(ht, name, strlen(name) + 1, &data) == SUCCESS) {
		ZVAL_LONG(*(zval**)data, Z_LVAL_PP((zval**)data) + count);
	} else {
		add_assoc_long(counts, name, count);
	}
}

/**
 * Looksup the hash table for the given symbol
 * Initializes a new array() if symbol is not present
 *
 * @author kannan, veeve
 */
zval * hp_hash_lookup(zval *hash, char *symbol  TSRMLS_DC)
{
	HashTable   *ht;
	void        *data;
	zval        *counts = (zval *) 0;

	/* Bail if something is goofy */
	if (!hash || !(ht = HASH_OF(hash))) {
		return (zval *) 0;
	}

	/* Lookup our hash table */
	if (zend_hash_find(ht, symbol, strlen(symbol) + 1, &data) == SUCCESS) {
		/* Symbol already exists */
		counts = *(zval **) data;
	}
	else {
		/* Add symbol to hash table */
		MAKE_STD_ZVAL(counts);
		array_init(counts);
		add_assoc_zval(hash, symbol, counts);
	}

	return counts;
}

/**
 * Truncates the given timeval to the nearest slot begin, where
 * the slot size is determined by intr
 *
 * @param  tv       Input timeval to be truncated in place
 * @param  intr     Time interval in microsecs - slot width
 * @return void
 * @author veeve
 */
void hp_trunc_time(struct timeval *tv, uint64 intr)
{
	uint64 time_in_micro;

	/* Convert to microsecs and trunc that first */
	time_in_micro = (tv->tv_sec * 1000000) + tv->tv_usec;
	time_in_micro /= intr;
	time_in_micro *= intr;

	/* Update tv */
	tv->tv_sec  = (time_in_micro / 1000000);
	tv->tv_usec = (time_in_micro % 1000000);
}

/**
 * ***********************
 * High precision timer related functions.
 * ***********************
 */

/**
 * Get time stamp counter (TSC) value via 'rdtsc' instruction.
 *
 * @return 64 bit unsigned integer
 * @author cjiang
 */
static inline uint64 cycle_timer() {
#ifdef __APPLE__
	return mach_absolute_time();
#else
	uint32 __a,__d;
	uint64 val;
	asm volatile("rdtsc" : "=a" (__a), "=d" (__d));
	(val) = ((uint64)__a) | (((uint64)__d)<<32);
	return val;
#endif
}

/**
 * Bind the current process to a specified CPU. This function is to ensure that
 * the OS won't schedule the process to different processors, which would make
 * values read by rdtsc unreliable.
 *
 * @param uint32 cpu_id, the id of the logical cpu to be bound to.
 * @return int, 0 on success, and -1 on failure.
 *
 * @author cjiang
 */
int bind_to_cpu(uint32 cpu_id)
{
	cpu_set_t new_mask;

	if (hp_globals.invariant_tsc) {
		return 0;
	}

	CPU_ZERO(&new_mask);
	CPU_SET(cpu_id, &new_mask);

	if (SET_AFFINITY(0, sizeof(cpu_set_t), &new_mask) < 0) {
		perror("setaffinity");
		return -1;
	}

	/* record the cpu_id the process is bound to. */
	hp_globals.cur_cpu_id = cpu_id;

	return 0;
}

/**
 * Check for invariant tsc cpuinfo flags.
 *
 * If the cpu is invariant to tsc drift, then we can skip binding
 * the request to a single processor, which is harmful to performance.
 */
static int is_invariant_tsc() {
#if defined(__x86_64__) || defined(__amd64__)
	unsigned int regs[4];

	asm volatile("cpuid" : "=a" (regs[0]), "=b" (regs[1]), "=c" (regs[2]), "=d" (regs[3]) : "a" (0), "c" (0));

	if ((regs[0] & 0x80000007) == 0) {
		return 0;
	}

	asm volatile("cpuid" : "=a" (regs[0]), "=b" (regs[1]), "=c" (regs[2]), "=d" (regs[3]) : "a" (0x80000007), "c" (0));

	if ((regs[3] & 0x00000100) == 0) {
		return 0;
	}

	return 1;
#else
	return 0;
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
 * Incr time with the given microseconds.
 */
static void incr_us_interval(struct timeval *start, uint64 incr)
{
	incr += (start->tv_sec * 1000000 + start->tv_usec);
	start->tv_sec  = incr/1000000;
	start->tv_usec = incr%1000000;

	return;
}

/**
 * Convert from TSC counter values to equivalent microseconds.
 *
 * @param uint64 count, TSC count value
 * @param double cpu_frequency, the CPU clock rate (MHz)
 * @return 64 bit unsigned integer
 *
 * @author cjiang
 */
static inline double get_us_from_tsc(uint64 count, double cpu_frequency)
{
	return count / cpu_frequency;
}

/**
 * Convert microseconds to equivalent TSC counter ticks
 *
 * @param uint64 microseconds
 * @param double cpu_frequency, the CPU clock rate (MHz)
 * @return 64 bit unsigned integer
 *
 * @author veeve
 */
static inline uint64 get_tsc_from_us(uint64 usecs, double cpu_frequency)
{
	return (uint64) (usecs * cpu_frequency);
}

/**
 * This is a microbenchmark to get cpu frequency the process is running on. The
 * returned value is used to convert TSC counter values to microseconds.
 *
 * @return double.
 * @author cjiang
 */
static double get_cpu_frequency()
{
#ifdef __APPLE__
	mach_timebase_info_data_t sTimebaseInfo;
	(void) mach_timebase_info(&sTimebaseInfo);

	return (sTimebaseInfo.numer / sTimebaseInfo.denom) * 1000;
#else
	struct timeval start;
	struct timeval end;

	if (gettimeofday(&start, 0)) {
		perror("gettimeofday");
		return 0.0;
	}
	uint64 tsc_start = cycle_timer();

	uint64 tsc_end;
	volatile int i;
	/* Busy loop for 5 miliseconds. */
	do {
		for (i = 0; i < 1000000; i++);
			if (gettimeofday(&end, 0)) {
				perror("gettimeofday");
				return 0.0;
			}
		tsc_end = cycle_timer();
	} while (get_us_interval(&start, &end) < 5000);

	return (tsc_end - tsc_start) * 1.0 / (get_us_interval(&start, &end));
#endif
}

/**
 * Calculate frequencies for all available cpus.
 *
 * @author cjiang
 */
static void get_all_cpu_frequencies()
{
	int id;
	double frequency;

	hp_globals.cpu_frequencies = malloc(sizeof(double) * hp_globals.cpu_num);
	if (hp_globals.cpu_frequencies == NULL) {
		return;
	}

	/* Iterate over all cpus found on the machine. */
	for (id = 0; id < hp_globals.cpu_num; ++id) {
		/* Only get the previous cpu affinity mask for the first call. */
		if (bind_to_cpu(id)) {
			clear_frequencies();
			return;
		}

		/* Make sure the current process gets scheduled to the target cpu. This
		 * might not be necessary though. */
		usleep(0);

		frequency = get_cpu_frequency();
		if (frequency == 0.0) {
			clear_frequencies();
			return;
		}
		hp_globals.cpu_frequencies[id] = frequency;
	}
}

/**
 * Restore cpu affinity mask to a specified value. It returns 0 on success and
 * -1 on failure.
 *
 * @param cpu_set_t * prev_mask, previous cpu affinity mask to be restored to.
 * @return int, 0 on success, and -1 on failure.
 *
 * @author cjiang
 */
int restore_cpu_affinity(cpu_set_t * prev_mask)
{
	if (hp_globals.invariant_tsc) {
		return 0;
	}

	if (SET_AFFINITY(0, sizeof(cpu_set_t), prev_mask) < 0) {
		perror("restore setaffinity");
		return -1;
	}

	/* default value ofor cur_cpu_id is 0. */
	hp_globals.cur_cpu_id = 0;

	return 0;
}

/**
 * Reclaim the memory allocated for cpu_frequencies.
 *
 * @author cjiang
 */
static void clear_frequencies()
{
	if (hp_globals.cpu_frequencies) {
		free(hp_globals.cpu_frequencies);
		hp_globals.cpu_frequencies = NULL;
	}

	restore_cpu_affinity(&hp_globals.prev_mask);
}

/**
 * TIDEWAYS_MODE_HIERARCHICAL's begin function callback
 *
 * @author kannan
 */
void hp_mode_hier_beginfn_cb(hp_entry_t **entries, hp_entry_t *current TSRMLS_DC)
{
	hp_entry_t   *p;

	/* This symbol's recursive level */
	int    recurse_level = 0;

	if (hp_globals.func_hash_counters[current->hash_code] > 0) {
		/* Find this symbols recurse level */
		for(p = (*entries); p; p = p->prev_hprof) {
			if (!strcmp(current->name_hprof, p->name_hprof)) {
				recurse_level = (p->rlvl_hprof) + 1;
				break;
			}
		}
	}
	hp_globals.func_hash_counters[current->hash_code]++;

	/* Init current function's recurse level */
	current->rlvl_hprof = recurse_level;

	/* Get start tsc counter */
	current->tsc_start = cycle_timer();
	current->gc_runs = GC_G(gc_runs);
	current->gc_collected = GC_G(collected);

	/* Get CPU usage */
	if (hp_globals.tideways_flags & TIDEWAYS_FLAGS_CPU) {
		getrusage(RUSAGE_SELF, &(current->ru_start_hprof));
	}

	/* Get memory usage */
	if (hp_globals.tideways_flags & TIDEWAYS_FLAGS_MEMORY) {
		current->mu_start_hprof  = zend_memory_usage(0 TSRMLS_CC);
		current->pmu_start_hprof = zend_memory_peak_usage(0 TSRMLS_CC);
	}
}

/**
 * **********************************
 * TIDEWAYS END FUNCTION CALLBACKS
 * **********************************
 */

/**
 * TIDEWAYS_MODE_HIERARCHICAL's end function callback
 *
 * @author kannan
 */
void hp_mode_hier_endfn_cb(hp_entry_t **entries, zend_execute_data *data TSRMLS_DC)
{
	hp_entry_t      *top = (*entries);
	zval            *counts;
	struct rusage    ru_end;
	char             symbol[SCRATCH_BUF_LEN] = "";
	long int         mu_end;
	long int         pmu_end;
	uint64   tsc_end;
	double   wt;
	/* Get the stat array */
	hp_get_function_stack(top, 2, symbol, sizeof(symbol));

	/* Get end tsc counter */
	tsc_end = cycle_timer();

	/* Get the stat array */
	if (!(counts = hp_hash_lookup(hp_globals.stats_count, symbol TSRMLS_CC))) {
		return;
	}

	wt = get_us_from_tsc(tsc_end - top->tsc_start, hp_globals.cpu_frequencies[hp_globals.cur_cpu_id]);

	tw_trace_callback *callback;
	if (data != NULL && zend_hash_find(hp_globals.trace_callbacks, top->name_hprof, strlen(top->name_hprof)+1, (void **)&callback) == SUCCESS) {
		void **args =  hp_get_execute_arguments(data);
		int arg_count = (int)(zend_uintptr_t) *args;
		zval *obj = data->object;
		double start = get_us_from_tsc(top->tsc_start - hp_globals.start_time, hp_globals.cpu_frequencies[hp_globals.cur_cpu_id]);
		double end = get_us_from_tsc(tsc_end - hp_globals.start_time, hp_globals.cpu_frequencies[hp_globals.cur_cpu_id]);

		(*callback)(symbol, args, arg_count, obj, start, end TSRMLS_CC);
	}

	/* Bump stats in the counts hashtable */
	hp_inc_count(counts, "ct", 1  TSRMLS_CC);
	hp_inc_count(counts, "wt", wt TSRMLS_CC);

	if ((GC_G(gc_runs) - top->gc_runs) > 0) {
		hp_inc_count(counts, "gc", GC_G(gc_runs) - top->gc_runs TSRMLS_CC);
		hp_inc_count(counts, "gcc", GC_G(collected) - top->gc_collected TSRMLS_CC);
	}

	if (hp_globals.tideways_flags & TIDEWAYS_FLAGS_CPU) {
		/* Get CPU usage */
		getrusage(RUSAGE_SELF, &ru_end);

		/* Bump CPU stats in the counts hashtable */
		hp_inc_count(counts, "cpu", (get_us_interval(&(top->ru_start_hprof.ru_utime),
						&(ru_end.ru_utime)) +
					get_us_interval(&(top->ru_start_hprof.ru_stime),
						&(ru_end.ru_stime)))
				TSRMLS_CC);
	}

	if (hp_globals.tideways_flags & TIDEWAYS_FLAGS_MEMORY) {
		/* Get Memory usage */
		mu_end  = zend_memory_usage(0 TSRMLS_CC);
		pmu_end = zend_memory_peak_usage(0 TSRMLS_CC);

		/* Bump Memory stats in the counts hashtable */
		hp_inc_count(counts, "mu",  mu_end - top->mu_start_hprof    TSRMLS_CC);
		hp_inc_count(counts, "pmu", pmu_end - top->pmu_start_hprof  TSRMLS_CC);
	}

	hp_globals.func_hash_counters[top->hash_code]--;
}


/**
 * ***************************
 * PHP EXECUTE/COMPILE PROXIES
 * ***************************
 */

/**
 * For transaction name detection in layer mode we only need a very simple user function overwrite.
 * Layer mode skips profiling userland functions, so we can simplify here.
 */
#if PHP_VERSION_ID < 50500
ZEND_DLEXPORT void hp_detect_tx_execute (zend_op_array *ops TSRMLS_DC) {
	zend_execute_data *execute_data = EG(current_execute_data);
	zend_execute_data *real_execute_data = execute_data;
#else
ZEND_DLEXPORT void hp_detect_tx_execute_ex (zend_execute_data *execute_data TSRMLS_DC) {
	zend_op_array *ops = execute_data->op_array;
	zend_execute_data    *real_execute_data = execute_data->prev_execute_data;
#endif
	char          *func = NULL;

	func = hp_get_function_name(real_execute_data TSRMLS_CC);
	if (func) {
		hp_detect_transaction_name(func, real_execute_data TSRMLS_CC);

		if (hp_globals.exception_function != NULL && strcmp(func, hp_globals.exception_function->value) == 0) {
			hp_detect_exception(func, real_execute_data TSRMLS_CC);
		}

		efree(func);
	}

#if PHP_VERSION_ID < 50500
	_zend_execute(ops TSRMLS_CC);
#else
	_zend_execute_ex(execute_data TSRMLS_CC);
#endif
}

/**
 * Tideways enable replaced the zend_execute function with this
 * new execute function. We can do whatever profiling we need to
 * before and after calling the actual zend_execute().
 *
 * @author hzhao, kannan
 */
#if PHP_VERSION_ID < 50500
ZEND_DLEXPORT void hp_execute (zend_op_array *ops TSRMLS_DC) {
	zend_execute_data *execute_data = EG(current_execute_data);
	zend_execute_data *real_execute_data = execute_data;
#else
ZEND_DLEXPORT void hp_execute_ex (zend_execute_data *execute_data TSRMLS_DC) {
	zend_op_array *ops = execute_data->op_array;
	zend_execute_data    *real_execute_data = execute_data->prev_execute_data;
#endif
	char          *func = NULL;
	int hp_profile_flag = 1;

	func = hp_get_function_name(real_execute_data TSRMLS_CC);
	if (!func) {
#if PHP_VERSION_ID < 50500
		_zend_execute(ops TSRMLS_CC);
#else
		_zend_execute_ex(execute_data TSRMLS_CC);
#endif
		return;
	}

	hp_detect_transaction_name(func, real_execute_data TSRMLS_CC);

	if (hp_globals.exception_function != NULL && strcmp(func, hp_globals.exception_function->value) == 0) {
		hp_detect_exception(func, real_execute_data TSRMLS_CC);
	}

	BEGIN_PROFILING(&hp_globals.entries, func, hp_profile_flag, real_execute_data);
#if PHP_VERSION_ID < 50500
	_zend_execute(ops TSRMLS_CC);
#else
	_zend_execute_ex(execute_data TSRMLS_CC);
#endif
	if (hp_globals.entries) {
		END_PROFILING(&hp_globals.entries, hp_profile_flag, real_execute_data);
	}
	efree(func);
}

#undef EX
#define EX(element) ((execute_data)->element)

/**
 * Very similar to hp_execute. Proxy for zend_execute_internal().
 * Applies to zend builtin functions.
 *
 * @author hzhao, kannan
 */

#if PHP_VERSION_ID < 50500
#define EX_T(offset) (*(temp_variable *)((char *) EX(Ts) + offset))

ZEND_DLEXPORT void hp_execute_internal(zend_execute_data *execute_data,
                                       int ret TSRMLS_DC) {
#else
#define EX_T(offset) (*EX_TMP_VAR(execute_data, offset))

ZEND_DLEXPORT void hp_execute_internal(zend_execute_data *execute_data,
                                       struct _zend_fcall_info *fci, int ret TSRMLS_DC) {
#endif
	char             *func = NULL;
	int    hp_profile_flag = 1;

	func = hp_get_function_name(execute_data TSRMLS_CC);

	if (func) {
		BEGIN_PROFILING(&hp_globals.entries, func, hp_profile_flag, execute_data);
	}

	if (!_zend_execute_internal) {
#if PHP_VERSION_ID < 50500
		execute_internal(execute_data, ret TSRMLS_CC);
#else
		execute_internal(execute_data, fci, ret TSRMLS_CC);
#endif
	} else {
		/* call the old override */
#if PHP_VERSION_ID < 50500
		_zend_execute_internal(execute_data, ret TSRMLS_CC);
#else
		_zend_execute_internal(execute_data, fci, ret TSRMLS_CC);
#endif
	}

	if (func) {
		if (hp_globals.entries) {
			END_PROFILING(&hp_globals.entries, hp_profile_flag, execute_data);
		}
		efree(func);
	}
}

/**
 * Proxy for zend_compile_file(). Used to profile PHP compilation time.
 *
 * @author kannan, hzhao
 */
ZEND_DLEXPORT zend_op_array* hp_compile_file(zend_file_handle *file_handle, int type TSRMLS_DC)
{
	const char     *filename;
	char           *func;
	int             len;
	zend_op_array  *ret;
	int             hp_profile_flag = 1;

	filename = hp_get_base_filename(file_handle->filename);
	len      = strlen("load") + strlen(filename) + 3;
	func      = (char *)emalloc(len);
	snprintf(func, len, "load::%s", filename);

	BEGIN_PROFILING(&hp_globals.entries, func, hp_profile_flag, NULL);
	ret = _zend_compile_file(file_handle, type TSRMLS_CC);
	if (hp_globals.entries) {
		END_PROFILING(&hp_globals.entries, hp_profile_flag, NULL);
	}

	efree(func);
	return ret;
}

/**
 * Proxy for zend_compile_string(). Used to profile PHP eval compilation time.
 */
ZEND_DLEXPORT zend_op_array* hp_compile_string(zval *source_string, char *filename TSRMLS_DC)
{
	char          *func;
	int            len;
	zend_op_array *ret;
	int            hp_profile_flag = 1;

	filename = (char *)hp_get_base_filename((const char *)filename);
	len  = strlen("eval") + strlen(filename) + 3;
	func = (char *)emalloc(len);
	snprintf(func, len, "eval::%s", filename);

	BEGIN_PROFILING(&hp_globals.entries, func, hp_profile_flag, NULL);
	ret = _zend_compile_string(source_string, filename TSRMLS_CC);
	if (hp_globals.entries) {
		END_PROFILING(&hp_globals.entries, hp_profile_flag, NULL);
	}

	efree(func);
	return ret;
}

/**
 * **************************
 * MAIN TIDEWAYS CALLBACKS
 * **************************
 */

/**
 * This function gets called once when Tideways gets enabled.
 * It replaces all the functions like zend_execute, zend_execute_internal,
 * etc that needs to be instrumented with their corresponding proxies.
 */
static void hp_begin(long tideways_flags TSRMLS_DC)
{
	if (!hp_globals.enabled) {
		int hp_profile_flag = 1;

		hp_globals.enabled      = 1;
		hp_globals.tideways_flags = (uint32)tideways_flags;

		/* Replace zend_compile file/string with our proxies */
		_zend_compile_file = zend_compile_file;
		_zend_compile_string = zend_compile_string;

		if (!(hp_globals.tideways_flags & TIDEWAYS_FLAGS_NO_COMPILE)) {
			zend_compile_file  = hp_compile_file;
			zend_compile_string = hp_compile_string;
		}

		/* Replace zend_execute with our proxy */
#if PHP_VERSION_ID < 50500
		_zend_execute = zend_execute;
#else
		_zend_execute_ex = zend_execute_ex;
#endif

		if (!(hp_globals.tideways_flags & TIDEWAYS_FLAGS_NO_USERLAND)) {
#if PHP_VERSION_ID < 50500
			zend_execute  = hp_execute;
#else
			zend_execute_ex  = hp_execute_ex;
#endif
		} else if (hp_globals.transaction_function) {
#if PHP_VERSION_ID < 50500
			zend_execute  = hp_detect_tx_execute;
#else
			zend_execute_ex  = hp_detect_tx_execute_ex;
#endif
		}

		tideways_original_error_cb = zend_error_cb;
		zend_error_cb = tideways_error_cb;

		/* Replace zend_execute_internal with our proxy */
		_zend_execute_internal = zend_execute_internal;
		if (!(hp_globals.tideways_flags & TIDEWAYS_FLAGS_NO_BUILTINS)) {
			/* if NO_BUILTINS is not set (i.e. user wants to profile builtins),
			 * then we intercept internal (builtin) function calls.
			 */
			zend_execute_internal = hp_execute_internal;
		}

		/* one time initializations */
		hp_init_profiler_state(TSRMLS_C);

		/* start profiling from fictitious main() */
		hp_globals.root = estrdup(ROOT_SYMBOL);
		BEGIN_PROFILING(&hp_globals.entries, hp_globals.root, hp_profile_flag, NULL);
	}
}

/**
 * Called at request shutdown time. Cleans the profiler's global state.
 */
static void hp_end(TSRMLS_D)
{
	/* Bail if not ever enabled */
	if (!hp_globals.ever_enabled) {
		return;
	}

	/* Stop profiler if enabled */
	if (hp_globals.enabled) {
		hp_stop(TSRMLS_C);
	}

	/* Clean up state */
	hp_clean_profiler_state(TSRMLS_C);
}

/**
 * Called from tideways_disable(). Removes all the proxies setup by
 * hp_begin() and restores the original values.
 */
static void hp_stop(TSRMLS_D)
{
	int hp_profile_flag = 1;

	/* End any unfinished calls */
	while (hp_globals.entries) {
		END_PROFILING(&hp_globals.entries, hp_profile_flag, NULL);
	}

	if (hp_globals.root) {
		efree(hp_globals.root);
		hp_globals.root = NULL;
	}

	/* Remove proxies, restore the originals */
#if PHP_VERSION_ID < 50500
	zend_execute = _zend_execute;
#else
	zend_execute_ex = _zend_execute_ex;
#endif

	zend_execute_internal = _zend_execute_internal;
	zend_compile_file     = _zend_compile_file;
	zend_compile_string   = _zend_compile_string;

	zend_error_cb = tideways_original_error_cb;

	/* Resore cpu affinity. */
	restore_cpu_affinity(&hp_globals.prev_mask);

	/* Stop profiling */
	hp_globals.enabled = 0;
}


/**
 * *****************************
 * TIDEWAYS ZVAL UTILITY FUNCTIONS
 * *****************************
 */

/** Look in the PHP assoc array to find a key and return the zval associated
 *  with it.
 *
 *  @author mpal
 **/
static zval *hp_zval_at_key(char  *key, zval  *values)
{
	zval *result = NULL;

	if (values->type == IS_ARRAY) {
		HashTable *ht;
		zval     **value;
		uint       len = strlen(key) + 1;

		ht = Z_ARRVAL_P(values);
		if (zend_hash_find(ht, key, len, (void**)&value) == SUCCESS) {
			result = *value;
		}
	}

	return result;
}

/**
 *  Convert the PHP array of strings to an emalloced array of strings. Note,
 *  this method duplicates the string data in the PHP array.
 *
 *  @author mpal
 **/
static char **hp_strings_in_zval(zval  *values)
{
	char   **result;
	size_t   count;
	size_t   ix = 0;

	if (!values) {
		return NULL;
	}

	if (values->type == IS_ARRAY) {
		HashTable *ht;

		ht    = Z_ARRVAL_P(values);
		count = zend_hash_num_elements(ht);

		if((result =
					(char**)emalloc(sizeof(char*) * (count + 1))) == NULL) {
			return result;
		}

		for (zend_hash_internal_pointer_reset(ht);
				zend_hash_has_more_elements(ht) == SUCCESS;
				zend_hash_move_forward(ht)) {
			char  *str;
			uint   len;
			ulong  idx;
			int    type;
			zval **data;

			type = zend_hash_get_current_key_ex(ht, &str, &len, &idx, 0, NULL);

			if (type == HASH_KEY_IS_LONG) {
				if ((zend_hash_get_current_data(ht, (void**)&data) == SUCCESS) &&
						Z_TYPE_PP(data) == IS_STRING &&
						strcmp(Z_STRVAL_PP(data), ROOT_SYMBOL)) { /* do not ignore "main" */
					result[ix] = estrdup(Z_STRVAL_PP(data));
					ix++;
				}
			} else if (type == HASH_KEY_IS_STRING) {
				result[ix] = estrdup(str);
				ix++;
			}
		}
	} else if(values->type == IS_STRING) {
		if((result = (char**)emalloc(sizeof(char*) * 2)) == NULL) {
			return result;
		}
		result[0] = estrdup(Z_STRVAL_P(values));
		ix = 1;
	} else {
		result = NULL;
	}

	/* NULL terminate the array */
	if (result != NULL) {
		result[ix] = NULL;
	}

	return result;
}

/* Free this memory at the end of profiling */
static inline void hp_array_del(char **name_array)
{
	if (name_array != NULL) {
		int i = 0;
		for(; name_array[i] != NULL && i < TIDEWAYS_MAX_FILTERED_FUNCTIONS; i++) {
			efree(name_array[i]);
		}
		efree(name_array);
	}
}

void tideways_error_cb(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args)
{
	TSRMLS_FETCH();
	error_handling_t  error_handling;
	zval *backtrace;

#if (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION >= 3) || PHP_MAJOR_VERSION >= 6
	error_handling  = EG(error_handling);
#else
	error_handling  = PG(error_handling);
#endif

	if (error_handling == EH_NORMAL) {
		switch (type) {
			case E_ERROR:
			case E_CORE_ERROR:
				ALLOC_INIT_ZVAL(backtrace);

#if PHP_VERSION_ID <= 50399
				zend_fetch_debug_backtrace(backtrace, 1, 0 TSRMLS_CC);
#else
				zend_fetch_debug_backtrace(backtrace, 1, 0, 0 TSRMLS_CC);
#endif

				hp_globals.backtrace = backtrace;
		}
	}

	tideways_original_error_cb(type, error_filename, error_lineno, format, args);
}

static inline hp_string *hp_create_string(const char *value, size_t length)
{
	hp_string *str;

	str = emalloc(sizeof(hp_string));
	str->value = estrdup(value);
	str->length = length;

	return str;
}

static inline long hp_zval_to_long(zval *z)
{
	if (Z_TYPE_P(z) == IS_LONG) {
		return Z_LVAL_P(z);
	}

	return 0;
}

static inline hp_string *hp_zval_to_string(zval *z)
{
	if (Z_TYPE_P(z) == IS_STRING) {
		return hp_create_string(Z_STRVAL_P(z), Z_STRLEN_P(z));
	}

	return NULL;
}

static inline zval *hp_string_to_zval(hp_string *str)
{
	zval *ret;
	char *val;

	MAKE_STD_ZVAL(ret);
	ZVAL_NULL(ret);

	if (str == NULL) {
		return ret;
	}

	ZVAL_STRINGL(ret, str->value, str->length, 1);

	return ret;
}


static inline void hp_string_clean(hp_string *str)
{
	if (str == NULL) {
		return;
	}

	efree(str->value);
}

