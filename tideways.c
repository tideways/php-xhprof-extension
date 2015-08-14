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

#if __APPLE__
#include <mach/mach_init.h>
#include <mach/mach_time.h>
#endif

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
#include "zend_smart_str.h"

#if PHP_VERSION_ID < 70000
struct _zend_string {
  char *val;
  int   len;
  int   persistent;
};
typedef struct _zend_string zend_string;
typedef long zend_long;
typedef ulong zend_ulong;
typedef int strsize_t;
typedef zend_uint uint32_t;

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

static inline int hp_num_execute_arguments(zend_execute_data *data)
{
	void **p = hp_get_execute_arguments(data);
	return (int)(zend_uintptr_t) *p;
}

static inline zval *hp_get_execute_argument(zend_execute_data *data, int n)
{
	void **p = hp_get_execute_arguments(data);
	int arg_count = (int)(zend_uintptr_t) *p;
	return *(p-(arg_count-n));
}

static zend_always_inline zend_string *zend_string_alloc(int len, int persistent)
{
	/* single alloc, so free the buf, will also free the struct */
	char *buf = safe_pemalloc(sizeof(zend_string)+len+1,1,0,persistent);
	zend_string *str = (zend_string *)(buf+len+1);

	str->val = buf;
	str->len = len;
	str->persistent = persistent;

	return str;
}

static zend_always_inline zend_string *zend_string_init(const char *str, size_t len, int persistent)
{
	zend_string *ret = zend_string_alloc(len, persistent);

	memcpy(ret->val, str, len);
	ret->val[len] = '\0';
	return ret;
}

static zend_always_inline void zend_string_free(zend_string *s)
{
	if (s == NULL) {
		return;
	}

	pefree(s->val, s->persistent);
}

static zend_always_inline void zend_string_release(zend_string *s)
{
	if (s == NULL) {
		return;
	}

	pefree(s->val, s->persistent);
}

/* new macros */
#define RETURN_STR_COPY(s)    RETURN_STRINGL(estrdup(s->val), s->len, 0)
#define Z_STR_P(z)			  zend_string_init(Z_STRVAL_P(z), Z_STRLEN_P(z), 0)
#define ZSTR_VAL(zstr)  (zstr)->val

#define ZEND_CALL_NUM_ARGS(call) hp_num_execute_arguments(call)
#define ZEND_CALL_ARG(call, n) hp_get_execute_argument(call, n-1)
#define EX_OBJ(call) call->object

#define _ZCE_NAME(ce) ce->name
#define _ZCE_NAME_LENGTH(ce) ce->name_length
#define _ZVAL_STRING(str, len) ZVAL_STRING(str, len, 0)
#define _RETURN_STRING(str) RETURN_STRING(str, 0)
#define _add_assoc_string_ex(arg, key, key_len, str, copy) add_assoc_string_ex(arg, key, key_len, str, copy)
#define _add_assoc_stringl(arg, key, str, str_len, copy) add_assoc_stringl(arg, key, str, str_len, copy)
#define _zend_read_property(scope, object, name, name_length, silent, zv) zend_read_property(scope, object, name, name_length, silent TSRMLS_CC)
#define _DECLARE_ZVAL(name) zval * name
#define _ALLOC_INIT_ZVAL(name) ALLOC_INIT_ZVAL(name)
#define hp_ptr_dtor(val) zval_ptr_dtor( &val )

#else
#define EX_OBJ(call) &(call->This)
#define _ZCE_NAME(ce) ce->name->val
#define _ZCE_NAME_LENGTH(ce) ce->name->len
#define _ZVAL_STRING(str, len) ZVAL_STRING(str, len)
#define _RETURN_STRING(str) RETURN_STRING(str)
#define _add_assoc_string_ex(arg, key, key_len, str, copy) add_assoc_string_ex(arg, key, key_len-1, str)
#define _add_assoc_stringl(arg, key, str, str_len, copy) add_assoc_stringl(arg, key, str, str_len)
#define _zend_read_property(scope, object, name, name_length, silent, zv) zend_read_property(scope, object, name, name_length, silent, zv)
#define _DECLARE_ZVAL(name) zval name ## _v; zval * name = &name ## _v
#define _ALLOC_INIT_ZVAL(name) ZVAL_NULL(name)
#define hp_ptr_dtor(val) zval_ptr_dtor(val)


typedef size_t strsize_t;
/* removed/uneeded macros */
#define TSRMLS_CC
#endif

static zend_always_inline void zend_compat_hash_merge(HashTable *target, HashTable *source, copy_ctor_func_t pCopyConstructor, zend_bool overwrite)
{
#if PHP_VERSION_ID < 70000
	zend_hash_merge(target, source, pCopyConstructor, NULL, sizeof(zval*), overwrite);
#else
	zend_hash_merge(target, source, pCopyConstructor, overwrite);
#endif
}

static zend_always_inline void zend_compat_hash_index_update(HashTable *ht, zend_ulong idx, zval *pData)
{
#if PHP_VERSION_ID < 70000
	zend_hash_index_update(ht, idx, &pData, sizeof(zval*), NULL);
#else
	zend_hash_index_update(ht, idx, pData);
#endif
}

static zend_always_inline void zend_compat_hash_update_ptr_const(HashTable *ht, const char *key, strsize_t len, void *ptr, size_t ptr_size)
{
#if PHP_VERSION_ID < 70000
	zend_hash_update(ht, key, len+1, ptr, ptr_size, NULL);
#else
	zend_hash_str_update_ptr(ht, key, len, ptr);
#endif
}

static zend_always_inline zval* zend_compat_hash_find_const(HashTable *ht, const char *key, strsize_t len)
{
#if PHP_VERSION_ID < 70000
	zval **tmp, *result;
	if (zend_hash_find(ht, key, len+1, (void**)&tmp) == SUCCESS) {
		result = *tmp;
		return result;
	}
	return NULL;
#else
	return zend_hash_str_find(ht, key, len);
#endif
}

static zend_always_inline zval* zend_compat_hash_index_find(HashTable *ht, zend_ulong idx)
{
#if PHP_VERSION_ID < 70000
	zval **tmp, *result;

	if (zend_hash_index_find(ht, idx, (void **) &tmp) == FAILURE) {
		return;
	}

	result = *tmp;
	return result;
#else
	return zend_hash_index_find(ht, idx);
#endif
}

/**
 * **********************
 * GLOBAL MACRO CONSTANTS
 * **********************
 */

/* Tideways version                           */
#define TIDEWAYS_VERSION       "2.0.8"

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
#define TIDEWAYS_FLAGS_NO_SPANS      0x0020
#define TIDEWAYS_FLAGS_NO_HIERACHICAL 0x0040

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

#define register_trace_callback(function_name, cb) zend_compat_hash_update_ptr_const(hp_globals.trace_callbacks, function_name, strlen(function_name), &cb, sizeof(tw_trace_callback*));
#define register_trace_callback_len(function_name, len, cb) zend_compat_hash_update_ptr_const(hp_globals.trace_callbacks, function_name, len, &cb, sizeof(tw_trace_callback*));

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
	uint64                  tsc_start;         /* start value for wall clock timer */
	uint64					cpu_start;		   /* start value for CPU clock timer */
	long int                mu_start_hprof;                    /* memory usage */
	long int                pmu_start_hprof;              /* peak memory usage */
	struct hp_entry_t      *prev_hprof;    /* ptr to prev entry being profiled */
	uint8                   hash_code;     /* hash_code for the function name  */
	long int				span_id; /* span id of this entry if any, otherwise -1 */
} hp_entry_t;

typedef struct hp_function_map {
	char **names;
	uint8 filter[TIDEWAYS_FILTERED_FUNCTION_SIZE];
} hp_function_map;

typedef struct tw_watch_callback {
	zend_fcall_info fci;
	zend_fcall_info_cache fcic;
} tw_watch_callback;

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
	HashTable       *stats_count;
	HashTable		*spans;
	long			current_span_id;
	uint64			start_time;

	zval			*backtrace;
	zval			*exception;

	/* Top of the profile stack */
	hp_entry_t      *entries;

	/* freelist of hp_entry_t chunks for reuse... */
	hp_entry_t      *entry_free_list;

	/* Function that determines the transaction name and callback */
	zend_string       *transaction_function;
	zend_string		*transaction_name;
	char			*root;

	zend_string		*exception_function;

	double timebase_factor;

	/* Tideways flags */
	uint32 tideways_flags;

	/* counter table indexed by hash value of function names. */
	uint8  func_hash_counters[256];

	/* Table of filtered function names and their filter */
	int     filtered_type; // 1 = blacklist, 2 = whitelist, 0 = nothing

	hp_function_map *filtered_functions;

	HashTable *trace_watch_callbacks;
	HashTable *trace_callbacks;
	HashTable *span_cache;
	int slow_php_call_treshold;

	uint32_t gc_runs; /* number of garbage collection runs */
	uint32_t gc_collected; /* number of collected items in garbage run */
	int compile_count;
	double compile_wt;
	uint64 cpu_start;
} hp_global_t;

typedef long (*tw_trace_callback)(char *symbol, zend_execute_data *data TSRMLS_DC);

/**
 * ***********************
 * GLOBAL STATIC VARIABLES
 * ***********************
 */
/* Tideways global state */
static hp_global_t       hp_globals;

#if PHP_VERSION_ID >= 70000
static ZEND_DLEXPORT void (*_zend_execute_ex) (zend_execute_data *execute_data);
static ZEND_DLEXPORT void (*_zend_execute_internal) (zend_execute_data *execute_data, zval *return_value);
#elif PHP_VERSION_ID < 50500
static ZEND_DLEXPORT void (*_zend_execute) (zend_op_array *ops TSRMLS_DC);
static ZEND_DLEXPORT void (*_zend_execute_internal) (zend_execute_data *data, int ret TSRMLS_DC);
#else
static void (*_zend_execute_ex) (zend_execute_data *execute_data TSRMLS_DC);
static void (*_zend_execute_internal) (zend_execute_data *data, struct _zend_fcall_info *fci, int ret TSRMLS_DC);
#endif

/* Pointer to the original compile function */
static zend_op_array * (*_zend_compile_file) (zend_file_handle *file_handle, int type TSRMLS_DC);

/* Pointer to the original compile string function (used by eval) */
static zend_op_array * (*_zend_compile_string) (zval *source_string, char *filename TSRMLS_DC);

ZEND_DLEXPORT zend_op_array* hp_compile_file(zend_file_handle *file_handle, int type TSRMLS_DC);
ZEND_DLEXPORT zend_op_array* hp_compile_string(zval *source_string, char *filename TSRMLS_DC);
#if PHP_MAJOR_VERSION == 7
ZEND_DLEXPORT void hp_execute_ex (zend_execute_data *execute_data);
#elif PHP_VERSION_ID < 50500
ZEND_DLEXPORT void hp_execute (zend_op_array *ops TSRMLS_DC);
#else
ZEND_DLEXPORT void hp_execute_ex (zend_execute_data *execute_data TSRMLS_DC);
#endif
#if PHP_MAJOR_VERSION == 7
ZEND_DLEXPORT void hp_execute_internal(zend_execute_data *execute_data, zval *return_value);
#elif PHP_VERSION_ID < 50500
ZEND_DLEXPORT void hp_execute_internal(zend_execute_data *execute_data, int ret TSRMLS_DC);
#else
ZEND_DLEXPORT void hp_execute_internal(zend_execute_data *execute_data, struct _zend_fcall_info *fci, int ret TSRMLS_DC);
#endif

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

static uint64 cycle_timer();

static void hp_free_the_free_list();
static hp_entry_t *hp_fast_alloc_hprof_entry();
static void hp_fast_free_hprof_entry(hp_entry_t *p);
static inline uint8 hp_inline_hash(char * str);
static double get_timebase_factor();
static long get_us_interval(struct timeval *start, struct timeval *end);
static inline double get_us_from_tsc(uint64 count);

static void hp_parse_options_from_arg(zval *args);
static void hp_clean_profiler_options_state();

static void hp_exception_function_clear();
static void hp_transaction_function_clear();
static void hp_transaction_name_clear();

static inline zval *hp_zval_at_key(char *key, size_t size, zval *values);
static inline char **hp_strings_in_zval(zval  *values);
static inline void   hp_array_del(char **name_array);
static char *hp_get_sql_summary(char *sql, int len TSRMLS_DC);
static char *hp_get_file_summary(char *filename, int filename_len TSRMLS_DC);
static const char *hp_get_base_filename(const char *filename);

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

ZEND_BEGIN_ARG_INFO_EX(arginfo_tideways_span_watch, 0, 0, 0)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, category)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_tideways_span_callback, 0, 0, 0)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, callback)
ZEND_END_ARG_INFO()

/* }}} */

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
	PHP_FE(tideways_span_watch, arginfo_tideways_span_watch)
	PHP_FE(tideways_span_callback, arginfo_tideways_span_callback)
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
PHP_INI_ENTRY("tideways.sample_rate", "30", PHP_INI_ALL, NULL)
PHP_INI_ENTRY("tideways.auto_prepend_library", "1", PHP_INI_ALL, NULL)
PHP_INI_ENTRY("tideways.collect", "tracing", PHP_INI_ALL, NULL)
PHP_INI_ENTRY("tideways.monitor", "basic", PHP_INI_ALL, NULL)
PHP_INI_ENTRY("tideways.distributed_tracing_hosts", "127.0.0.1", PHP_INI_ALL, NULL)

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
	zend_long tideways_flags = 0;
	zval *optional_array = NULL;

	if (hp_globals.enabled) {
		hp_stop(TSRMLS_C);
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
	if (!hp_globals.enabled) {
		return;
	}

	hp_stop(TSRMLS_C);

	RETURN_ARR(zend_array_dup(hp_globals.stats_count));
}

PHP_FUNCTION(tideways_transaction_name)
{
	if (hp_globals.transaction_name) {
		RETURN_STR_COPY(hp_globals.transaction_name);
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
		add_assoc_long_ex(return_value, "type", sizeof("type")-1, PG(last_error_type));
		_add_assoc_string_ex(return_value, "message", sizeof("message"), PG(last_error_message), 1);
		_add_assoc_string_ex(return_value, "file", sizeof("file"), PG(last_error_file)?PG(last_error_file):"-", 1);
		add_assoc_long_ex(return_value, "line", sizeof("line")-1, PG(last_error_lineno));
	}
}

long tw_span_create(char *category, size_t category_len)
{
	zval span, starts, stops, annotations;
	int idx;
	long parent = 0;

	idx = zend_hash_num_elements(hp_globals.spans);

	array_init(&span);
	array_init(&starts);
	array_init(&stops);
	array_init(&annotations);

	_add_assoc_stringl(&span, "n", category, category_len, 1);
	add_assoc_zval(&span, "b", &starts);
	add_assoc_zval(&span, "e", &stops);
	add_assoc_zval(&span, "a", &annotations);

	if (parent > 0) {
		add_assoc_long(&span, "p", parent);
	}

	zend_compat_hash_index_update(hp_globals.spans, idx, &span);

	return idx;
}

void tw_span_timer_start(long spanId)
{
	zval *span, *starts;
	double wt;

	span = zend_hash_index_find(hp_globals.spans, spanId);

	if (span == NULL) {
		return;
	}

	starts = zend_compat_hash_find_const(Z_ARRVAL_P(span), "b", 1);

	if (starts == NULL) {
		return;
	}

	wt = get_us_from_tsc(cycle_timer() - hp_globals.start_time);
	add_next_index_long(starts, wt);
}

void tw_span_record_duration(long spanId, double start, double end)
{
	zval *span, *timer;

	span = zend_compat_hash_index_find(hp_globals.spans, spanId);

	if (span == NULL) {
		return;
	}

	timer = zend_compat_hash_find_const(Z_ARRVAL_P(span), "b", 1);

	if (timer == NULL) {
		return;
	}

	add_next_index_long(timer, start);

	timer = zend_compat_hash_find_const(Z_ARRVAL_P(span), "e", 1);

	if (timer == NULL) {
		return;
	}

	add_next_index_long(timer, end);
}

void tw_span_timer_stop(long spanId)
{
	zval *span, *stops;
	double wt;

	span = zend_compat_hash_index_find(hp_globals.spans, spanId);

	if (span == NULL) {
		return;
	}

	stops = zend_compat_hash_find_const(Z_ARRVAL_P(span), "e", 1);

	if (stops == NULL) {
		return;
	}

	wt = get_us_from_tsc(cycle_timer() - hp_globals.start_time);
	add_next_index_long(stops, wt);
}

#if PHP_VERSION_ID < 70000
static int tw_convert_to_string(void *pDest TSRMLS_DC)
{
	zval **zv = (zval **) pDest;
#else
static int tw_convert_to_string(zval *pDest TSRMLS_DC)
{
	zval *zv = pDest;
#endif

	convert_to_string_ex(zv);

	return ZEND_HASH_APPLY_KEEP;
}

void tw_span_annotate(long spanId, zval *annotations TSRMLS_DC)
{
	zval *span, *span_annotations;

	span = zend_compat_hash_index_find(hp_globals.spans, spanId);

	if (span == NULL) {
		return;
	}

	span_annotations = zend_compat_hash_find_const(Z_ARRVAL_P(span), "a", sizeof("a") - 1);

	if (span_annotations == NULL) {
		return;
	}

	zend_hash_apply(Z_ARRVAL_P(annotations), tw_convert_to_string TSRMLS_CC);

	zend_compat_hash_merge(Z_ARRVAL_P(span_annotations), Z_ARRVAL_P(annotations), (copy_ctor_func_t) zval_add_ref, 1);
}

void tw_span_annotate_long(long spanId, char *key, long value)
{
	zval *span, *span_annotations;
	_DECLARE_ZVAL(annotation_value);

	span = zend_compat_hash_index_find(hp_globals.spans, spanId);

	if (span == NULL) {
		return;
	}

	span_annotations = zend_compat_hash_find_const(Z_ARRVAL_P(span), "a", sizeof("a") - 1);

	if (span_annotations == NULL) {
		return;
	}

	_ALLOC_INIT_ZVAL(annotation_value);
	ZVAL_LONG(annotation_value, value);
#if PHP_VERSION_ID < 70000
	convert_to_string_ex(&annotation_value);
#else
	convert_to_string_ex(annotation_value);
#endif

	add_assoc_zval_ex(span_annotations, key, strlen(key), annotation_value);
}

void tw_span_annotate_string(long spanId, char *key, char *value, int copy)
{
	zval *span, *span_annotations;

	span = zend_compat_hash_index_find(hp_globals.spans, spanId);

	if (span == NULL) {
		return;
	}

	span_annotations = zend_compat_hash_find_const(Z_ARRVAL_P(span), "a", sizeof("a") - 1);

	if (span_annotations == NULL) {
		return;
	}

	_add_assoc_string_ex(span_annotations, key, strlen(key), value, copy);
}

PHP_FUNCTION(tideways_span_create)
{
	char *category = NULL;
	strsize_t category_len;

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
	if (hp_globals.spans) {
		RETURN_ARR(zend_array_dup(hp_globals.spans));
	}
}

PHP_FUNCTION(tideways_span_timer_start)
{
	zend_long spanId;

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
	zend_long spanId;

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
	zend_long spanId;
	zval *annotations;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "lz", &spanId, &annotations) == FAILURE) {
		return;
	}

	// Yes, annotations are still possible when profiler is deactivated!
	tw_span_annotate(spanId, annotations TSRMLS_CC);
}

PHP_FUNCTION(tideways_sql_minify)
{
	char *sql, *minified;
	strsize_t len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &sql, &len) == FAILURE) {
		return;
	}

	minified = hp_get_sql_summary(sql, len TSRMLS_CC);

	_RETURN_STRING(minified);
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
	hp_globals.slow_php_call_treshold = 50000; // 50ms
	hp_globals.timebase_factor = get_timebase_factor();

	hp_globals.stats_count = NULL;
	hp_globals.spans = NULL;
	hp_globals.trace_callbacks = NULL;
	hp_globals.trace_watch_callbacks = NULL;
	hp_globals.span_cache = NULL;

	/* no free hp_entry_t structures to start with */
	hp_globals.entry_free_list = NULL;

	for (i = 0; i < 256; i++) {
		hp_globals.func_hash_counters[i] = 0;
	}

	hp_transaction_function_clear();
	hp_exception_function_clear();

	_zend_compile_file = zend_compile_file;
	zend_compile_file  = hp_compile_file;
	_zend_compile_string = zend_compile_string;
	zend_compile_string = hp_compile_string;

#if PHP_VERSION_ID < 50500
	_zend_execute = zend_execute;
	zend_execute  = hp_execute;
#else
	_zend_execute_ex = zend_execute_ex;
	zend_execute_ex  = hp_execute_ex;
#endif

	tideways_original_error_cb = zend_error_cb;
	zend_error_cb = tideways_error_cb;

	_zend_execute_internal = zend_execute_internal;
	zend_execute_internal = hp_execute_internal;

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
	/* free any remaining items in the free list */
	hp_free_the_free_list();

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

	UNREGISTER_INI_ENTRIES();

	return SUCCESS;
}

long tw_trace_callback_record_with_cache(char *category, int category_len, char *summary, int summary_len, int copy)
{
	long idx, *idx_ptr = NULL;

#if PHP_VERSION_ID < 70000
	if (zend_hash_find(hp_globals.span_cache, summary, strlen(summary)+1, (void **)&idx_ptr) == SUCCESS) {
		idx = *idx_ptr;
	} else {
		idx = tw_span_create(category, category_len);
		zend_hash_update(hp_globals.span_cache, summary, strlen(summary)+1, &idx, sizeof(long), NULL);
	}
#else
	if (idx_ptr = zend_hash_str_find_ptr(hp_globals.span_cache, summary, strlen(summary))) {
		idx = *idx_ptr;
	} else {
		idx = tw_span_create(category, category_len);
		zend_hash_str_update_ptr(hp_globals.span_cache, summary, strlen(summary), &idx);
	}
#endif

	tw_span_annotate_string(idx, "title", summary, copy);

	return idx;
}

long tw_trace_callback_php_call(char *symbol, zend_execute_data *data TSRMLS_DC)
{
	long idx;

	idx = tw_span_create("php", 3);
	tw_span_annotate_string(idx, "title", symbol, 1);

	return idx;
}

long tw_trace_callback_watch(char *symbol, zend_execute_data *data TSRMLS_DC)
{
	tw_watch_callback **temp;
	tw_watch_callback *twcb = NULL;
	zend_fcall_info fci = empty_fcall_info;
	zend_fcall_info_cache fcic = empty_fcall_info_cache;
	int args_len = ZEND_CALL_NUM_ARGS(data);
	zval *object = EX_OBJ(data);

	if (hp_globals.trace_watch_callbacks == NULL) {
		return -1;
	}

#if PHP_VERSION_ID < 70000
	if (zend_hash_find(hp_globals.trace_watch_callbacks, symbol, strlen(symbol)+1, (void **)&temp) == SUCCESS) {
		twcb = *temp;
#else
	twcb = zend_hash_str_find_ptr(hp_globals.trace_watch_callbacks, symbol, strlen(symbol));

	if (twcb) {
#endif
		zval *retval = NULL;
		_DECLARE_ZVAL(context);
		_DECLARE_ZVAL(zargs);
		zval *params[1];
		zend_error_handling zeh;
		int i;

		_ALLOC_INIT_ZVAL(context);
		array_init(context);

		_ALLOC_INIT_ZVAL(zargs);
		array_init(zargs);
		Z_ADDREF_P(zargs);

		_add_assoc_string_ex(context, "fn", sizeof("fn"), symbol, 1);

		if (args_len > 0) {
			for (i = 0; i < args_len; i++) {
				Z_ADDREF_P(ZEND_CALL_ARG(data, i+1));
				add_next_index_zval(zargs, ZEND_CALL_ARG(data, i+1));
			}
		}

		add_assoc_zval(context, "args", zargs);

		if (object != NULL) {
			Z_ADDREF_P(object);
			add_assoc_zval(context, "object", object);
		}

		params[0] = (zval *)&(context);

		twcb->fci.param_count = 1;
		twcb->fci.size = sizeof(twcb->fci);
#if PHP_VERSION_ID < 70000
		twcb->fci.retval_ptr_ptr = &retval;
#else
		twcb->fci.retval = retval;
#endif
		twcb->fci.params = (zval ***)params;

		if (zend_call_function(&(twcb->fci), &(twcb->fcic) TSRMLS_CC) == FAILURE) {
			zend_error(E_ERROR, "Cannot call Trace Watch Callback");
		}

		hp_ptr_dtor(context);
		hp_ptr_dtor(zargs);

		long idx = -1;

		if (retval) {
			if (Z_TYPE_P(retval) == IS_LONG) {
				idx = Z_LVAL_P(retval);
			}

			hp_ptr_dtor(retval);
		}

		return idx;
	}

	return -1;
}

long tw_trace_callback_memcache(char *symbol, zend_execute_data *data TSRMLS_DC)
{
	return tw_trace_callback_record_with_cache("memcache", 8, symbol, strlen(symbol), 1);
}

long tw_trace_callback_php_controller(char *symbol, zend_execute_data *data TSRMLS_DC)
{
	long idx;

	idx = tw_span_create("php.ctrl", 8);
	tw_span_annotate_string(idx, "title", symbol, 1);

	return idx;
}

long tw_trace_callback_doctrine_couchdb_request(char *symbol, zend_execute_data *data TSRMLS_DC)
{
	zval *method = ZEND_CALL_ARG(data, 1);
	zval *path = ZEND_CALL_ARG(data, 2);
	long idx;
	zval title, tmp, space;

	if (Z_TYPE_P(method) != IS_STRING || Z_TYPE_P(path) != IS_STRING) {
		return -1;
	}

	_ZVAL_STRING(&space, " ");

	concat_function(&tmp, method, &space TSRMLS_CC);
	concat_function(&title, &tmp, path TSRMLS_CC);

	idx = tw_span_create("http.couchdb", 12);
	tw_span_annotate_string(idx, "title", Z_STRVAL(title), 0);

	efree(Z_STRVAL(tmp));

	return idx;
}

/* Mage_Core_Block_Abstract::toHtml() */
long tw_trace_callback_magento_block(char *symbol, zend_execute_data *data TSRMLS_DC)
{
	zend_class_entry *ce;
	zval *object = EX_OBJ(data);

	if (object == NULL || Z_TYPE_P(object) != IS_OBJECT) {
		return -1;
	}

	ce = Z_OBJCE_P(object);

	return tw_trace_callback_record_with_cache("view", 4, _ZCE_NAME(ce), _ZCE_NAME_LENGTH(ce), 1);
}

/* Zend_View_Abstract::render($name); */
long tw_trace_callback_view_engine(char *symbol, zend_execute_data *data TSRMLS_DC)
{
	zval *name = ZEND_CALL_ARG(data, 1);
	char *view;

	if (Z_TYPE_P(name) != IS_STRING) {
		return -1;
	}

	view = hp_get_base_filename(Z_STRVAL_P(name));

	return tw_trace_callback_record_with_cache("view", 4, view, strlen(view)+1, 1);
}

/* Applies to Enlight, Mage and Zend1 */
long tw_trace_callback_zend1_dispatcher_families_tx(char *symbol, zend_execute_data *data TSRMLS_DC)
{
	zval *argument_element = ZEND_CALL_ARG(data, 1);
	int len;
	char *ret = NULL;
	zend_class_entry *ce;
	long idx;
	zval *object = EX_OBJ(data);

	if (object == NULL || Z_TYPE_P(object) != IS_OBJECT) {
		return -1;
	}

	if (Z_TYPE_P(argument_element) != IS_STRING) {
		return -1;
	}

	ce = Z_OBJCE_P(object);

	len = _ZCE_NAME_LENGTH(ce) + Z_STRLEN_P(argument_element) + 3;
	ret = (char*)emalloc(len);
	snprintf(ret, len, "%s::%s", _ZCE_NAME(ce), Z_STRVAL_P(argument_element));

	idx = tw_span_create("php.ctrl", 8);
	tw_span_annotate_string(idx, "title", ret, 0);

	return idx;
}

/* oxShopControl::_process($sClass, $sFnc = null); */
long tw_trace_callback_oxid_tx(char *symbol, zend_execute_data *data TSRMLS_DC)
{
	zval *sClass = ZEND_CALL_ARG(data, 1);
	zval *sFnc = ZEND_CALL_ARG(data, 2);
	char *ret = NULL;
	int len, copy;
	int args_len = ZEND_CALL_NUM_ARGS(data);

	if (Z_TYPE_P(sClass) != IS_STRING) {
		return -1;
	}

	if (args_len > 1 && sFnc != NULL && Z_TYPE_P(sFnc) == IS_STRING) {
		len = Z_STRLEN_P(sClass) + Z_STRLEN_P(sFnc) + 3;
		ret = (char*)emalloc(len);
		snprintf(ret, len, "%s::%s", Z_STRVAL_P(sClass), Z_STRVAL_P(sFnc));
		copy = 0;
	} else {
		ret = Z_STRVAL_P(sClass);
		len = Z_STRLEN_P(sClass);
		copy = 1;
	}

	if ((hp_globals.tideways_flags & TIDEWAYS_FLAGS_NO_SPANS) > 0) {
		return -1;
	}

	return tw_trace_callback_record_with_cache("php.ctrl", 8, ret, len, copy);
}

/* $resolver->getArguments($request, $controller); */
long tw_trace_callback_symfony_resolve_arguments_tx(char *symbol, zend_execute_data *data TSRMLS_DC)
{
	zval *callback, *controller, *action;
	char *ret = NULL;
	int len;
	tw_trace_callback cb;
	zend_class_entry *ce;

	callback = ZEND_CALL_ARG(data, 2);

	// Only Symfony2 framework for now
	if (Z_TYPE_P(callback) == IS_ARRAY) {
		controller = zend_compat_hash_index_find(Z_ARRVAL_P(callback), 0);

		if (controller == NULL && Z_TYPE_P(controller) != IS_OBJECT) {
			return -1;
		}

		action = zend_compat_hash_index_find(Z_ARRVAL_P(callback), 1);

		if (action == NULL && Z_TYPE_P(action) != IS_STRING) {
			return -1;
		}

		ce = Z_OBJCE_P(controller);

		len = _ZCE_NAME_LENGTH(ce) + Z_STRLEN_P(action) + 3;
		ret = (char*)emalloc(len);
		snprintf(ret, len, "%s::%s", _ZCE_NAME(ce), Z_STRVAL_P(action));

		cb = tw_trace_callback_php_controller;
		register_trace_callback_len(ret, len-1, cb);

		efree(ret);
	}

	return -1;
}

long tw_trace_callback_pgsql_execute(char *symbol, zend_execute_data *data TSRMLS_DC)
{
	zval *argument_element;
	char *summary;
	int i;
	int args_len = ZEND_CALL_NUM_ARGS(data);

	for (i = 0; i < args_len; i++) {
		argument_element = ZEND_CALL_ARG(data, i+1);

		if (argument_element && Z_TYPE_P(argument_element) == IS_STRING && Z_STRLEN_P(argument_element) > 0) {
			// TODO: Introduce SQL statement cache to find the names here again.
			summary = Z_STRVAL_P(argument_element);

			return tw_trace_callback_record_with_cache("sql", 3, summary, strlen(summary), 1);
		}
	}

	return -1;
}

long tw_trace_callback_pgsql_query(char *symbol, zend_execute_data *data TSRMLS_DC)
{
	zval *argument_element;
	char *summary;
	int i;
	int args_len = ZEND_CALL_NUM_ARGS(data);

	for (i = 0; i < args_len; i++) {
		argument_element = ZEND_CALL_ARG(data, i+1);

		if (argument_element && Z_TYPE_P(argument_element) == IS_STRING) {
			summary = hp_get_sql_summary(Z_STRVAL_P(argument_element), Z_STRLEN_P(argument_element) TSRMLS_CC);

			return tw_trace_callback_record_with_cache("sql", 3, summary, strlen(summary), 0);
		}
	}

	return -1;
}

long tw_trace_callback_smarty3_template(char *symbol, zend_execute_data *data TSRMLS_DC)
{
	zval *argument_element = ZEND_CALL_ARG(data, 1);
	zval *obj;
	zend_class_entry *smarty_ce;
	char *template;
	size_t template_len;

	if (argument_element && Z_TYPE_P(argument_element) == IS_STRING) {
		template = Z_STRVAL_P(argument_element);
	} else {
		zval *object = EX_OBJ(data);

		if (object == NULL || Z_TYPE_P(object) != IS_OBJECT) {
			return -1;
		}

		smarty_ce = Z_OBJCE_P(object);

		zval *__rv;
		argument_element = _zend_read_property(smarty_ce, object, "template_resource", sizeof("template_resource") - 1, 1, __rv);

		if (Z_TYPE_P(argument_element) != IS_STRING) {
			return -1;
		}

		template = Z_STRVAL_P(argument_element);
	}

	template_len = Z_STRLEN_P(argument_element);

	return tw_trace_callback_record_with_cache("view", 4, template, template_len, 1);
}

long tw_trace_callback_doctrine_persister(char *symbol, zend_execute_data *data TSRMLS_DC)
{
	zval *property;
	zend_class_entry *persister_ce, *metadata_ce;
	zval *object = EX_OBJ(data);

	if (object == NULL || Z_TYPE_P(object) != IS_OBJECT) {
		return -1;
	}

	persister_ce = Z_OBJCE_P(object);

	zval *__rv;
	property = _zend_read_property(persister_ce, object, "class", sizeof("class") - 1, 1, __rv);
	if (property == NULL) {
		property = _zend_read_property(persister_ce, object, "_class", sizeof("_class") - 1, 1, __rv);
	}

	if (property != NULL && Z_TYPE_P(property) == IS_OBJECT) {
		metadata_ce = Z_OBJCE_P(property);

		property = _zend_read_property(metadata_ce, property, "name", sizeof("name") - 1, 1, __rv);

		if (property == NULL) {
			return -1;
		}

		return tw_trace_callback_record_with_cache("doctrine.load", 13, Z_STRVAL_P(property), Z_STRLEN_P(property), 1);
	}

	return -1;
}

long tw_trace_callback_doctrine_query(char *symbol, zend_execute_data *data TSRMLS_DC)
{
	zval *property;
	zend_class_entry *query_ce;
	zval fname, *retval_ptr;
	char *summary;
	long idx = -1;
	zval *object = EX_OBJ(data);

	if (object == NULL || Z_TYPE_P(object) != IS_OBJECT) {
		return idx;
	}

	query_ce = Z_OBJCE_P(object);

	if (strcmp(_ZCE_NAME(query_ce), "Doctrine\\ORM\\Query") == 0) {
		_ZVAL_STRING(&fname, "getDQL");
	} else if (strcmp(_ZCE_NAME(query_ce), "Doctrine\\ORM\\NativeQuery") == 0) {
		_ZVAL_STRING(&fname, "getSQL");
	} else {
		return idx;
	}

	if (SUCCESS == call_user_function_ex(EG(function_table), &object, &fname, &retval_ptr, 0, NULL, 1, NULL TSRMLS_CC)) {
		if (Z_TYPE_P(retval_ptr) != IS_STRING) {
			return idx;
		}

		summary = hp_get_sql_summary(Z_STRVAL_P(retval_ptr), Z_STRLEN_P(retval_ptr) TSRMLS_CC);

		idx = tw_trace_callback_record_with_cache("doctrine.query", 14, summary, strlen(summary), 0);

		hp_ptr_dtor(retval_ptr);
	}

	return idx;
}

long tw_trace_callback_twig_template(char *symbol, zend_execute_data *data TSRMLS_DC)
{
	long idx = -1, *idx_ptr;
	zval fname, *retval_ptr;
	zval *object = EX_OBJ(data);

	if (object == NULL || Z_TYPE_P(object) != IS_OBJECT) {
		return idx;
	}

	_ZVAL_STRING(&fname, "getTemplateName");

	if (SUCCESS == call_user_function_ex(EG(function_table), &object, &fname, &retval_ptr, 0, NULL, 1, NULL TSRMLS_CC)) {
		if (Z_TYPE_P(retval_ptr) == IS_STRING) {
			idx = tw_trace_callback_record_with_cache("view", 4, Z_STRVAL_P(retval_ptr), Z_STRLEN_P(retval_ptr), 1);
		}

		hp_ptr_dtor(retval_ptr);
	}

	return idx;
}

long tw_trace_callback_event_dispatchers(char *symbol, zend_execute_data *data TSRMLS_DC)
{
	long idx = -1, *idx_ptr;
	zval *argument_element = ZEND_CALL_ARG(data, 1);

	if (argument_element && Z_TYPE_P(argument_element) == IS_STRING) {
		idx = tw_trace_callback_record_with_cache("event", 5, Z_STRVAL_P(argument_element), Z_STRLEN_P(argument_element), 1);
	}

	return idx;
}

long tw_trace_callback_pdo_stmt_execute(char *symbol, zend_execute_data *data TSRMLS_DC)
{
	pdo_stmt_t *stmt = (pdo_stmt_t*)zend_object_store_get_object_by_handle(Z_OBJ_HANDLE_P(EX_OBJ(data)) TSRMLS_CC);
	char *summary = hp_get_sql_summary(stmt->query_string, stmt->query_stringlen TSRMLS_CC);

	return tw_trace_callback_record_with_cache("sql", 3, summary, strlen(summary), 0);
}

long tw_trace_callback_mysqli_stmt_execute(char *symbol, zend_execute_data *data TSRMLS_DC)
{
	return tw_trace_callback_record_with_cache("sql", 3, "execute", 7, 1);
}

long tw_trace_callback_sql_commit(char *symbol, zend_execute_data *data TSRMLS_DC)
{
	return tw_trace_callback_record_with_cache("sql", 3, "commit", 3, 1);
}

long tw_trace_callback_sql_functions(char *symbol, zend_execute_data *data TSRMLS_DC)
{
	zval *argument_element;
	char *summary;

	if (strcmp(symbol, "mysqli_query") == 0 || strcmp(symbol, "mysqli_prepare") == 0) {
		argument_element = ZEND_CALL_ARG(data, 2);
	} else {
		argument_element = ZEND_CALL_ARG(data, 1);
	}

	if (Z_TYPE_P(argument_element) != IS_STRING) {
		return -1;
	}

	summary = hp_get_sql_summary(Z_STRVAL_P(argument_element), Z_STRLEN_P(argument_element) TSRMLS_CC);

	return tw_trace_callback_record_with_cache("sql", 3, summary, strlen(summary), 0);
}

long tw_trace_callback_fastcgi_finish_request(char *symbol, zend_execute_data *data TSRMLS_DC)
{
	// stop the main span, the request ended here
	tw_span_timer_stop(0);
	return -1;
}

long tw_trace_callback_curl_exec(char *symbol, zend_execute_data *data TSRMLS_DC)
{
	zval *argument = ZEND_CALL_ARG(data, 1);
	zval *option;
	zval ***params_array;
	char *summary;
	long idx, *idx_ptr;
	zval fname, *retval_ptr, *opt;

	if (argument == NULL || Z_TYPE_P(argument) != IS_RESOURCE) {
		return -1;
	}

	_ZVAL_STRING(&fname, "curl_getinfo");

	params_array = (zval ***) emalloc(sizeof(zval **));
	params_array[0] = &argument;

	if (SUCCESS == call_user_function_ex(EG(function_table), NULL, &fname, &retval_ptr, 1, params_array, 1, NULL TSRMLS_CC)) {
		option = zend_compat_hash_find_const(Z_ARRVAL_P(retval_ptr), "url", sizeof("url")-1);

		if (option && Z_TYPE_P(option) == IS_STRING) {
			summary = hp_get_file_summary(Z_STRVAL_P(option), Z_STRLEN_P(option) TSRMLS_CC);

			return tw_trace_callback_record_with_cache("http", 4, summary, strlen(summary), 0);
		}

		hp_ptr_dtor(retval_ptr);
	}

	efree(params_array);

	return -1;
}

long tw_trace_callback_soap_client_dorequest(char *symbol, zend_execute_data *data TSRMLS_DC)
{
	zval *argument = ZEND_CALL_ARG(data, 2);
	char *summary;

	if (Z_TYPE_P(argument) != IS_STRING) {
		return -1;
	}

	if (strncmp(Z_STRVAL_P(argument), "http", 4) != 0) {
		return -1;
	}

	summary = hp_get_file_summary(Z_STRVAL_P(argument), Z_STRLEN_P(argument) TSRMLS_CC);
	return tw_trace_callback_record_with_cache("http.soap", 9, summary, strlen(summary), 0);
}

long tw_trace_callback_file_get_contents(char *symbol, zend_execute_data *data TSRMLS_DC)
{
	zval *argument = ZEND_CALL_ARG(data, 1);
	char *summary;

	if (Z_TYPE_P(argument) != IS_STRING) {
		return -1;
	}

	if (strncmp(Z_STRVAL_P(argument), "http", 4) != 0) {
		return -1;
	}

	summary = hp_get_file_summary(Z_STRVAL_P(argument), Z_STRLEN_P(argument) TSRMLS_CC);
	return tw_trace_callback_record_with_cache("http", 4, summary, strlen(summary), 0);
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

	hp_globals.prepend_overwritten = 0;
	hp_globals.backtrace = NULL;
	hp_globals.exception = NULL;
	hp_globals.transaction_name = NULL;
	hp_globals.transaction_function = NULL;

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
	char *extension_dir;
	char *profiler_file;
	int profiler_file_len;

	php_info_print_table_start();
	php_info_print_table_header(2, "tideways", TIDEWAYS_VERSION);

	php_info_print_table_row(2, "Connection (tideways.connection)", INI_STR("tideways.connection"));
	php_info_print_table_row(2, "UDP Connection (tideways.udp_connection)", INI_STR("tideways.udp_connection"));
	php_info_print_table_row(2, "Default API Key (tideways.api_key)", INI_STR("tideways.api_key"));
	php_info_print_table_row(2, "Default Sample-Rate (tideways.sample_rate)", INI_STR("tideways.sample_rate"));
	php_info_print_table_row(2, "Framework Detection (tideways.framework)", INI_STR("tideways.framework"));
	php_info_print_table_row(2, "Automatically Start (tideways.auto_start)", INI_INT("tideways.auto_start") ? "Yes": "No");
	php_info_print_table_row(2, "Tideways Collect Mode (tideways.collect)", INI_STR("tideways.collect"));
	php_info_print_table_row(2, "Tideways Monitoring Mode (tideways.monitor)", INI_STR("tideways.monitor"));
	php_info_print_table_row(2, "Allowed Distributed Tracing Hosts (tideways.distributed_tracing_hosts)", INI_STR("tideways.distributed_tracing_hosts"));
	php_info_print_table_row(2, "Load PHP Library (tideways.auto_prepend_library)", INI_INT("tideways.auto_prepend_library") ? "Yes": "No");

	extension_dir  = INI_STR("extension_dir");
	profiler_file_len = strlen(extension_dir) + strlen("Tideways.php") + 2;
	profiler_file = emalloc(profiler_file_len);
	snprintf(profiler_file, profiler_file_len, "%s/%s", extension_dir, "Tideways.php");

	if (VCWD_ACCESS(profiler_file, F_OK) == 0) {
		php_info_print_table_row(2, "Tideways.php found", "Yes");
	} else {
		php_info_print_table_row(2, "Tideways.php found", "No");
	}

	efree(profiler_file);

	php_info_print_table_end();
}


/**
 * ***************************************************
 * COMMON HELPER FUNCTION DEFINITIONS AND LOCAL MACROS
 * ***************************************************
 */

static void hp_register_constants(INIT_FUNC_ARGS)
{
	REGISTER_LONG_CONSTANT("TIDEWAYS_FLAGS_CPU", TIDEWAYS_FLAGS_CPU, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("TIDEWAYS_FLAGS_MEMORY", TIDEWAYS_FLAGS_MEMORY, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("TIDEWAYS_FLAGS_NO_BUILTINS", TIDEWAYS_FLAGS_NO_BUILTINS, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("TIDEWAYS_FLAGS_NO_USERLAND", TIDEWAYS_FLAGS_NO_USERLAND, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("TIDEWAYS_FLAGS_NO_COMPILE", TIDEWAYS_FLAGS_NO_COMPILE, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("TIDEWAYS_FLAGS_NO_SPANS", TIDEWAYS_FLAGS_NO_SPANS, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("TIDEWAYS_FLAGS_NO_HIERACHICAL", TIDEWAYS_FLAGS_NO_HIERACHICAL, CONST_CS | CONST_PERSISTENT);
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

	zresult = hp_zval_at_key("ignored_functions", sizeof("ignored_functions"), args);

	if (zresult == NULL) {
		zresult = hp_zval_at_key("functions", sizeof("functions"), args);
		if (zresult != NULL) {
			hp_globals.filtered_type = 2;
		}
	} else {
		hp_globals.filtered_type = 1;
	}

	hp_globals.filtered_functions = hp_function_map_create(hp_strings_in_zval(zresult));

	zresult = hp_zval_at_key("transaction_function", sizeof("transaction_function"), args);

	if (zresult != NULL && Z_TYPE_P(zresult) == IS_STRING) {
		hp_globals.transaction_function = zend_string_copy(Z_STR_P(zresult));
	}

	zresult = hp_zval_at_key("exception_function", sizeof("exception_function"), args);

	if (zresult != NULL && Z_TYPE_P(zresult) == IS_STRING) {
		hp_globals.exception_function = zend_string_copy(Z_STR_P(zresult));
	}

	zresult = hp_zval_at_key("slow_php_call_treshold", sizeof("slow_php_call_treshold"), args);
	if (zresult != NULL && Z_TYPE_P(zresult) == IS_LONG && Z_LVAL_P(zresult) >= 0) {
		hp_globals.slow_php_call_treshold = Z_LVAL_P(zresult);
	}
}

static void hp_exception_function_clear() {
	if (hp_globals.exception_function != NULL) {
		zend_string_release(hp_globals.exception_function);
		hp_globals.exception_function = NULL;
	}

	if (hp_globals.exception != NULL) {
		hp_ptr_dtor(hp_globals.exception);
	}
}

static void hp_transaction_function_clear() {
	if (hp_globals.transaction_function) {
		zend_string_release(hp_globals.transaction_function);
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

void hp_init_trace_callbacks(TSRMLS_D)
{
	tw_trace_callback cb;

	if ((hp_globals.tideways_flags & TIDEWAYS_FLAGS_NO_SPANS) > 0) {
		return;
	}

	hp_globals.trace_callbacks = NULL;
	hp_globals.trace_watch_callbacks = NULL;
	hp_globals.span_cache = NULL;

	ALLOC_HASHTABLE(hp_globals.trace_callbacks);
	zend_hash_init(hp_globals.trace_callbacks, 255, NULL, NULL, 0);

	ALLOC_HASHTABLE(hp_globals.span_cache);
	zend_hash_init(hp_globals.span_cache, 255, NULL, NULL, 0);

	cb = tw_trace_callback_file_get_contents;
	register_trace_callback("file_get_contents", cb);

	cb = tw_trace_callback_php_call;
	register_trace_callback("session_start", cb);
	// Symfony
	register_trace_callback("Symfony\\Component\\HttpKernel\\Kernel::boot", cb);
	register_trace_callback("Symfony\\Component\\EventDispatcher\\ContainerAwareEventDispatcher::lazyLoad", cb);
	// Wordpress
	register_trace_callback("get_sidebar", cb);
	register_trace_callback("get_header", cb);
	register_trace_callback("get_footer", cb);
	register_trace_callback("load_textdomain", cb);
	register_trace_callback("setup_theme", cb);
	// Doctrine
	register_trace_callback("Doctrine\\ORM\\EntityManager::flush", cb);
	register_trace_callback("Doctrine\\ODM\\CouchDB\\DocumentManager::flush", cb);
	// Magento
	register_trace_callback("Mage_Core_Model_App::_initModules", cb);
	register_trace_callback("Mage_Core_Model_Config::loadModules", cb);
	register_trace_callback("Mage_Core_Model_Config::loadDb", cb);
	// Smarty&Twig Compiler
	register_trace_callback("Smarty_Internal_TemplateCompilerBase::compileTemplate", cb);
	register_trace_callback("Twig_Environment::compileSource", cb);
	// Shopware Assets (very special, do we really need it?)
	register_trace_callback("JSMin::minify", cb);
	register_trace_callback("Less_Parser::getCss", cb);
	// Laravel (4+5)
	register_trace_callback("Illuminate\\Foundation\\Application::boot", cb);
	register_trace_callback("Illuminate\\Foundation\\Application::dispatch", cb);
	// Silex
	register_trace_callback("Silex\\Application::mount", cb);

	cb = tw_trace_callback_doctrine_persister;
	register_trace_callback("Doctrine\\ORM\\Persisters\\BasicEntityPersister::load", cb);
	register_trace_callback("Doctrine\\ORM\\Persisters\\BasicEntityPersister::loadAll", cb);
	register_trace_callback("Doctrine\\ORM\\Persisters\\Entity\\BasicEntityPersister::load", cb);
	register_trace_callback("Doctrine\\ORM\\Persisters\\Entity\\BasicEntityPersister::loadAll", cb);

	cb = tw_trace_callback_doctrine_query;
	register_trace_callback("Doctrine\\ORM\\AbstractQuery::execute", cb);

	cb = tw_trace_callback_doctrine_couchdb_request;
	register_trace_callback("Doctrine\\CouchDB\\HTTP\\SocketClient::request", cb);
	register_trace_callback("Doctrine\\CouchDB\\HTTP\\StreamClient::request", cb);

	cb = tw_trace_callback_curl_exec;
	register_trace_callback("curl_exec", cb);

	cb = tw_trace_callback_sql_functions;
	register_trace_callback("PDO::exec", cb);
	register_trace_callback("PDO::query", cb);
	register_trace_callback("mysql_query", cb);
	register_trace_callback("mysqli_query", cb);
	register_trace_callback("mysqli::query", cb);
	register_trace_callback("mysqli::prepare", cb);
	register_trace_callback("mysqli_prepare", cb);

	cb = tw_trace_callback_sql_commit;
	register_trace_callback("PDO::commit", cb);
	register_trace_callback("mysqli::commit", cb);
	register_trace_callback("mysqli_commit", cb);

	cb = tw_trace_callback_pdo_stmt_execute;
	register_trace_callback("PDOStatement::execute", cb);

	cb = tw_trace_callback_mysqli_stmt_execute;
	register_trace_callback("mysqli_stmt_execute", cb);
	register_trace_callback("mysqli_stmt::execute", cb);

	cb = tw_trace_callback_pgsql_query;
	register_trace_callback("pg_query", cb);
	register_trace_callback("pg_query_params", cb);

	cb = tw_trace_callback_pgsql_execute;
	register_trace_callback("pg_execute", cb);

	cb = tw_trace_callback_event_dispatchers;
	register_trace_callback("Doctrine\\Common\\EventManager::dispatchEvent", cb);
	register_trace_callback("Enlight_Event_EventManager::filter", cb);
	register_trace_callback("Enlight_Event_EventManager::notify", cb);
	register_trace_callback("Enlight_Event_EventManager::notifyUntil", cb);
	register_trace_callback("Zend\\EventManager\\EventManager::trigger", cb);
	register_trace_callback("do_action", cb);
	register_trace_callback("drupal_alter", cb);
	register_trace_callback("Mage::dispatchEvent", cb);
	register_trace_callback("Symfony\\Component\\EventDispatcher\\EventDispatcher::dispatch", cb);
	register_trace_callback("Illuminate\\Events\\Dispatcher::fire", cb);

	cb = tw_trace_callback_twig_template;
	register_trace_callback("Twig_Template::render", cb);
	register_trace_callback("Twig_Template::display", cb);

	cb = tw_trace_callback_smarty3_template;
	register_trace_callback("Smarty_Internal_TemplateBase::fetch", cb);

	cb = tw_trace_callback_fastcgi_finish_request;
	register_trace_callback("fastcgi_finish_request", cb);

	cb = tw_trace_callback_soap_client_dorequest;
	register_trace_callback("SoapClient::__doRequest", cb);

	cb = tw_trace_callback_magento_block;
	register_trace_callback("Mage_Core_Block_Abstract::toHtml", cb);

	cb = tw_trace_callback_view_engine;
	register_trace_callback("Zend_View_Abstract::render", cb);
	register_trace_callback("Illuminate\\View\\Engines\\CompilerEngine::get", cb);
	register_trace_callback("Smarty::fetch", cb);
	register_trace_callback("load_template", cb);

	cb = tw_trace_callback_zend1_dispatcher_families_tx;
	register_trace_callback("Enlight_Controller_Action::dispatch", cb);
	register_trace_callback("Mage_Core_Controller_Varien_Action::dispatch", cb);
	register_trace_callback("Zend_Controller_Action::dispatch", cb);
	register_trace_callback("Illuminate\\Routing\\Controller::callAction", cb);

	cb = tw_trace_callback_symfony_resolve_arguments_tx;
	register_trace_callback("Symfony\\Component\\HttpKernel\\Controller\\ControllerResolver::getArguments", cb);

	cb = tw_trace_callback_oxid_tx;
	register_trace_callback("oxShopControl::_process", cb);

	// Different versions of Memcache Extension have either MemcachePool or Memcache class, @todo investigate
	cb = tw_trace_callback_memcache;
	register_trace_callback("MemcachePool::get", cb);
	register_trace_callback("MemcachePool::set", cb);
	register_trace_callback("MemcachePool::delete", cb);
	register_trace_callback("MemcachePool::flush", cb);
	register_trace_callback("MemcachePool::replace", cb);
	register_trace_callback("MemcachePool::increment", cb);
	register_trace_callback("MemcachePool::decrement", cb);
	register_trace_callback("Memcache::get", cb);
	register_trace_callback("Memcache::set", cb);
	register_trace_callback("Memcache::delete", cb);
	register_trace_callback("Memcache::flush", cb);
	register_trace_callback("Memcache::replace", cb);
	register_trace_callback("Memcache::increment", cb);
	register_trace_callback("Memcache::decrement", cb);

	hp_globals.gc_runs = GC_G(gc_runs);
	hp_globals.gc_collected = GC_G(collected);
	hp_globals.compile_count = 0;
	hp_globals.compile_wt = 0;
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
		zend_hash_destroy(hp_globals.stats_count);
		FREE_HASHTABLE(hp_globals.stats_count);
	}

	ALLOC_HASHTABLE(hp_globals.stats_count);
	zend_hash_init(hp_globals.stats_count, 0, NULL, ZVAL_PTR_DTOR, 0);

	if (hp_globals.spans) {
		zend_hash_destroy(hp_globals.spans);
		FREE_HASHTABLE(hp_globals.spans);
	}

	ALLOC_HASHTABLE(hp_globals.spans);
	zend_hash_init(hp_globals.spans, 0, NULL, ZVAL_PTR_DTOR, 0);

	hp_init_trace_callbacks(TSRMLS_C);
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
		zend_hash_destroy(hp_globals.stats_count);
		FREE_HASHTABLE(hp_globals.stats_count);
		hp_globals.stats_count = NULL;
	}
	if (hp_globals.spans) {
		zend_hash_destroy(hp_globals.spans);
		FREE_HASHTABLE(hp_globals.spans);
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
		zend_string_release(hp_globals.transaction_name);
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

	if (hp_globals.trace_callbacks) {
		zend_hash_destroy(hp_globals.trace_callbacks);
		FREE_HASHTABLE(hp_globals.trace_callbacks);
		hp_globals.trace_callbacks = NULL;
	}

	if (hp_globals.trace_watch_callbacks) {
		zend_hash_destroy(hp_globals.trace_watch_callbacks);
		FREE_HASHTABLE(hp_globals.trace_watch_callbacks);
		hp_globals.trace_watch_callbacks = NULL;
	}

	if (hp_globals.span_cache) {
		zend_hash_destroy(hp_globals.span_cache);
		FREE_HASHTABLE(hp_globals.span_cache);
		hp_globals.span_cache = NULL;
	}
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
			(cur_entry)->span_id = -1;											\
			hp_mode_hier_beginfn_cb((entries), (cur_entry), execute_data TSRMLS_CC);			\
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
	result_buf[result_len - 1] = '\0';

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
	zval **data, *val, *tmp;
	_DECLARE_ZVAL(parts);
	HashTable *arrayParts;
	pcre_cache_entry	*pce;			/* Compiled regular expression */
	HashPosition pointer;
	int array_count, result_len, found, found_select;
	char *result, *token;
	char *key;
	int key_len;
	long index;

	found = 0;
	found_select = 0;
	_ALLOC_INIT_ZVAL(parts);

#if PHP_VERSION_ID < 70000
	if ((pce = pcre_get_compiled_regex_cache("(([\\s]+))", 8 TSRMLS_CC)) == NULL) {
		return "";
	}
#else
	zend_string *regex = zend_string_init("(([\\s]+))", 9, 0);
	if ((pce = pcre_get_compiled_regex_cache(regex)) == NULL) {
		zend_string_release(regex);
		return "";
	}
	zend_string_release(regex);
#endif

	php_pcre_split_impl(pce, sql, len, parts, -1, 0 TSRMLS_CC);

	arrayParts = Z_ARRVAL_P(parts);

	smart_str buf = {0};

#if PHP_VERSION_ID < 70000
	for(zend_hash_internal_pointer_reset_ex(arrayParts, &pointer);
			zend_hash_get_current_data_ex(arrayParts, (void**) &data, &pointer) == SUCCESS;
			zend_hash_move_forward_ex(arrayParts, &pointer)) {

		zend_hash_get_current_key_ex(arrayParts, &key, &key_len, &index, 0, &pointer);
		token = Z_STRVAL_PP(data);
		php_strtolower(token, Z_STRLEN_PP(data));
#else
	ZEND_HASH_FOREACH_KEY_VAL(arrayParts, index, key, val) {
		token = Z_STRVAL_P(val);
		php_strtolower(token, Z_STRLEN_P(val));
#endif


		if ((strcmp(token, "insert") == 0 || strcmp(token, "delete") == 0) &&
				zend_hash_index_exists(arrayParts, index+2)) {
			smart_str_appends(&buf, token);
			smart_str_appendc(&buf, ' ');

			tmp = zend_compat_hash_index_find(arrayParts, index+2);
			smart_str_appends(&buf, Z_STRVAL_P(tmp));
			found = 1;

			break;
		} else if (strcmp(token, "update") == 0 && zend_hash_index_exists(arrayParts, index+1)) {
			smart_str_appends(&buf, token);
			smart_str_appendc(&buf, ' ');

			tmp = zend_compat_hash_index_find(arrayParts, index+1);
			smart_str_appends(&buf, Z_STRVAL_P(tmp));
			found = 1;

			break;
		} else if (strcmp(token, "select") == 0) {
			smart_str_appends(&buf, token);
			smart_str_appendc(&buf, ' ');
			found_select = 1;
		} else if (found_select == 1 && strcmp(token, "from") == 0) {
			tmp = zend_compat_hash_index_find(arrayParts, index+1);
			smart_str_appends(&buf, Z_STRVAL_P(tmp));
			found = 1;

			break;
		} else if (strcmp(token, "commit") == 0) {
			smart_str_appends(&buf, token);
			found = 1;
			break;
		}
#if PHP_VERSION_ID < 70000
	}
#else
	} ZEND_HASH_FOREACH_END();
#endif

	hp_ptr_dtor(parts);

	if (found == 0) {
		smart_str_appends(&buf, "other");
	}
	smart_str_0(&buf);

	result = ZSTR_VAL(buf.s);
	smart_str_free(&buf);

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

static char *hp_concat_char(const char *s1, size_t len1, const char *s2, size_t len2, const char *seperator, size_t sep_len)
{
    char *result = emalloc(len1+len2+sep_len+1);

    strcpy(result, s1);
	strcat(result, seperator);
    strcat(result, s2);
	result[len1+len2+sep_len] = '\0';

    return result;
}

static void hp_detect_exception(char *func_name, zend_execute_data *data TSRMLS_DC)
{
	int arg_count = ZEND_CALL_NUM_ARGS(data);
	zval *argument_element;
	int i;
	zend_class_entry *default_ce, *exception_ce;

	default_ce = zend_exception_get_default(TSRMLS_C);

	for (i=0; i < arg_count; i++) {
		argument_element = ZEND_CALL_ARG(data, i+1);

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
		strcmp(ret, ZSTR_VAL(hp_globals.transaction_function)) != 0) {
		return;
	}

	zval *argument_element;

	if (strcmp(ret, "Zend_Controller_Action::dispatch") == 0 ||
			   strcmp(ret, "Enlight_Controller_Action::dispatch") == 0 ||
			   strcmp(ret, "Mage_Core_Controller_Varien_Action::dispatch") == 0 ||
			   strcmp(ret, "Illuminate\\Routing\\Controller::callAction") == 0) {
		zval *obj = EX_OBJ(data);
		argument_element = ZEND_CALL_ARG(data, 1);
		zend_class_entry *ce;
		int len;
		char *ctrl;

		if (Z_TYPE_P(argument_element) == IS_STRING) {
			ce = Z_OBJCE_P(obj);

			len = _ZCE_NAME_LENGTH(ce) + Z_STRLEN_P(argument_element) + 3;
			ctrl = (char*)emalloc(len);
			snprintf(ctrl, len, "%s::%s", _ZCE_NAME(ce), Z_STRVAL_P(argument_element));
			ctrl[len-1] = '\0';

			hp_globals.transaction_name = zend_string_init(ctrl, len-1, 0);
			efree(ctrl);
		}
	} else {
		argument_element = ZEND_CALL_ARG(data, 1);

		if (Z_TYPE_P(argument_element) == IS_STRING) {
			hp_globals.transaction_name = Z_STR_P(argument_element);
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
	const char        *cls = NULL;
	char              *ret = NULL;
	zend_function      *curr_func;

	if (!data) {
		return NULL;
	}

#if PHP_VERSION_ID < 70000
	const char        *func = NULL;
	curr_func = data->function_state.function;
	func = curr_func->common.function_name;

	if (!func) {
		// This branch includes execution of eval and include/require(_once) calls
		// We assume it is not 1999 anymore and not much PHP code runs in the
		// body of a file and if it is, we are ok with adding it to the caller's wt.
		return NULL;
	}

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
#else
	zend_string *func = NULL;
	curr_func = data->func;
	func = curr_func->common.function_name;

	if (!func) {
		return NULL;
	} else if (curr_func->common.scope != NULL) {
		char* sep = "::";
		cls = curr_func->common.scope->name->val;
		ret = hp_concat_char(cls, curr_func->common.scope->name->len, func->val, func->len, sep, 2);
	} else {
		ret = emalloc(ZSTR_LEN(func)+1);
		strcpy(ret, ZSTR_VAL(func));
		ret[ZSTR_LEN(func)] = '\0';
	}
#endif

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
	zval *data, val;

	if (!counts) {
		return;
	}

	ht = HASH_OF(counts);

	if (!ht) {
		return;
	}

	data = zend_compat_hash_find_const(ht, name, strlen(name));

	if (data) {
		ZVAL_LONG(data, Z_LVAL_P(data) + count);
	} else {
		ZVAL_LONG(&val, count);
		zend_hash_str_update(ht, name, strlen(name), &val);
	}
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
 * Get the current wallclock timer
 *
 * @return 64 bit unsigned integer
 * @author cjiang
 */
static uint64 cycle_timer() {
#ifdef __APPLE__
	return mach_absolute_time();
#else
	struct timespec s;
	clock_gettime(CLOCK_MONOTONIC, &s);

	return s.tv_sec * 1000000 + s.tv_nsec / 1000;
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

	getrusage(RUSAGE_SELF, &ru);

	return ru.ru_utime.tv_sec * 1000000 + ru.ru_utime.tv_usec +
		ru.ru_stime.tv_sec * 1000000 + ru.ru_stime.tv_usec;
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
 * Convert from TSC counter values to equivalent microseconds.
 *
 * @param uint64 count, TSC count value
 * @return 64 bit unsigned integer
 *
 * @author cjiang
 */
static inline double get_us_from_tsc(uint64 count)
{
	return count / hp_globals.timebase_factor;
}

/**
 * Get the timebase factor necessary to divide by in cycle_timer()
 */
static double get_timebase_factor()
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
 * TIDEWAYS_MODE_HIERARCHICAL's begin function callback
 *
 * @author kannan
 */
void hp_mode_hier_beginfn_cb(hp_entry_t **entries, hp_entry_t *current, zend_execute_data *data TSRMLS_DC)
{
	hp_entry_t   *p;
	tw_trace_callback *callback;
	int    recurse_level = 0;

	if ((hp_globals.tideways_flags & TIDEWAYS_FLAGS_NO_HIERACHICAL) == 0) {
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
	}

	/* Get start tsc counter */
	current->tsc_start = cycle_timer();

	if ((hp_globals.tideways_flags & TIDEWAYS_FLAGS_NO_SPANS) == 0 && data != NULL) {
#if PHP_VERSION_ID < 70000
		if (zend_hash_find(hp_globals.trace_callbacks, current->name_hprof, strlen(current->name_hprof)+1, (void **)&callback) == SUCCESS) {
			current->span_id = (*callback)(current->name_hprof, data TSRMLS_CC);
		}
#else
		callback = zend_hash_str_find_ptr(hp_globals.trace_callbacks, current->name_hprof, strlen(current->name_hprof));

		if (callback) {
			current->span_id = (*callback)(current->name_hprof, data TSRMLS_CC);
		}
#endif
	}

	/* Get CPU usage */
	if (hp_globals.tideways_flags & TIDEWAYS_FLAGS_CPU) {
		current->cpu_start = cpu_timer();
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
	zval            *counts, count_val;
	char             symbol[SCRATCH_BUF_LEN] = "";
	long int         mu_end;
	long int         pmu_end;
	uint64   tsc_end;
	double   wt, cpu;
	tw_trace_callback *callback;

	/* Get end tsc counter */
	tsc_end = cycle_timer();
	wt = get_us_from_tsc(tsc_end - top->tsc_start);

	if (hp_globals.tideways_flags & TIDEWAYS_FLAGS_CPU) {
		cpu = get_us_from_tsc(cpu_timer() - top->cpu_start);
	}

	if ((hp_globals.tideways_flags & TIDEWAYS_FLAGS_NO_SPANS) == 0) {
		if (top->span_id >= 0) {
			double start = get_us_from_tsc(top->tsc_start - hp_globals.start_time);
			double end = get_us_from_tsc(tsc_end - hp_globals.start_time);
			tw_span_record_duration(top->span_id, start, end);
#if PHP_VERSION_ID < 70000
		} else if (data != NULL && data->function_state.function->type == ZEND_INTERNAL_FUNCTION && wt > hp_globals.slow_php_call_treshold) {
			long idx = tw_trace_callback_php_call(top->name_hprof, data TSRMLS_CC);

			double start = get_us_from_tsc(top->tsc_start - hp_globals.start_time);
			double end = get_us_from_tsc(tsc_end - hp_globals.start_time);
			tw_span_record_duration(idx, start, end);
#endif
		}
	}

	if ((hp_globals.tideways_flags & TIDEWAYS_FLAGS_NO_HIERACHICAL) > 0) {
		return;
	}

	/* Get the stat array */
	hp_get_function_stack(top, 2, symbol, sizeof(symbol));

	/* Get the stat array */
	counts = zend_hash_str_find(hp_globals.stats_count, symbol, strlen(symbol));

	if (counts == NULL) {
		counts = &count_val;
		array_init(counts);
		zend_hash_str_update(hp_globals.stats_count, symbol, strlen(symbol), counts);
	}

	/* Bump stats in the counts hashtable */
	hp_inc_count(counts, "ct", 1  TSRMLS_CC);
	hp_inc_count(counts, "wt", wt TSRMLS_CC);

	if (hp_globals.tideways_flags & TIDEWAYS_FLAGS_CPU) {
		/* Bump CPU stats in the counts hashtable */
		hp_inc_count(counts, "cpu", cpu TSRMLS_CC);
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
 * Tideways enable replaced the zend_execute function with this
 * new execute function. We can do whatever profiling we need to
 * before and after calling the actual zend_execute().
 *
 * @author hzhao, kannan
 */
#if PHP_VERSION_ID >= 70000
ZEND_DLEXPORT void hp_execute_ex (zend_execute_data *execute_data) {
	zend_execute_data *real_execute_data = execute_data;
#elif PHP_VERSION_ID < 50500
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

	if (!hp_globals.enabled) {
#if PHP_VERSION_ID < 50500
		_zend_execute(ops TSRMLS_CC);
#else
		_zend_execute_ex(execute_data TSRMLS_CC);
#endif
		return;
	}

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

	if (hp_globals.exception_function != NULL && strcmp(func, ZSTR_VAL(hp_globals.exception_function)) == 0) {
		hp_detect_exception(func, real_execute_data TSRMLS_CC);
	}

	if ((hp_globals.tideways_flags & TIDEWAYS_FLAGS_NO_USERLAND) > 0) {
#if PHP_VERSION_ID < 50500
		_zend_execute(ops TSRMLS_CC);
#else
		_zend_execute_ex(execute_data TSRMLS_CC);
#endif
		efree(func);
		return;
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

#if PHP_VERSION_ID >= 70000
ZEND_DLEXPORT void hp_execute_internal(zend_execute_data *execute_data, zval *return_value) {
#elif PHP_VERSION_ID < 50500
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

	if (!hp_globals.enabled || (hp_globals.tideways_flags & TIDEWAYS_FLAGS_NO_BUILTINS) > 0) {
#if PHP_MAJOR_VERSION == 7
		execute_internal(execute_data, return_value TSRMLS_CC);
#elif PHP_VERSION_ID < 50500
		execute_internal(execute_data, ret TSRMLS_CC);
#else
		execute_internal(execute_data, fci, ret TSRMLS_CC);
#endif
		return;
	}

	func = hp_get_function_name(execute_data TSRMLS_CC);

	if (func) {
		BEGIN_PROFILING(&hp_globals.entries, func, hp_profile_flag, execute_data);
	}

	if (!_zend_execute_internal) {
#if PHP_VERSION_ID >= 70000
		execute_internal(execute_data, return_value TSRMLS_CC);
#elif PHP_VERSION_ID < 50500
		execute_internal(execute_data, ret TSRMLS_CC);
#else
		execute_internal(execute_data, fci, ret TSRMLS_CC);
#endif
	} else {
		/* call the old override */
#if PHP_VERSION_ID >= 70000
		_zend_execute_internal(execute_data, return_value TSRMLS_CC);
#elif PHP_VERSION_ID < 50500
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
	if (!hp_globals.enabled || (hp_globals.tideways_flags & TIDEWAYS_FLAGS_NO_COMPILE) > 0) {
		return _zend_compile_file(file_handle, type TSRMLS_CC);
	}

	zend_op_array  *ret;
	uint64 start = cycle_timer();

	hp_globals.compile_count++;

	ret = _zend_compile_file(file_handle, type TSRMLS_CC);

	hp_globals.compile_wt += get_us_from_tsc(cycle_timer() - start);

	return ret;
}

/**
 * Proxy for zend_compile_string(). Used to profile PHP eval compilation time.
 */
ZEND_DLEXPORT zend_op_array* hp_compile_string(zval *source_string, char *filename TSRMLS_DC)
{
	if (!hp_globals.enabled || (hp_globals.tideways_flags & TIDEWAYS_FLAGS_NO_COMPILE) > 0) {
		return _zend_compile_string(source_string, filename TSRMLS_CC);
	}

	zend_op_array  *ret;
	uint64 start = cycle_timer();

	hp_globals.compile_count++;

	ret = _zend_compile_string(source_string, filename TSRMLS_CC);

	hp_globals.compile_wt += get_us_from_tsc(cycle_timer() - start);

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

		/* one time initializations */
		hp_init_profiler_state(TSRMLS_C);

		/* start profiling from fictitious main() */
		hp_globals.root = estrdup(ROOT_SYMBOL);
		hp_globals.start_time = cycle_timer();

		if ((hp_globals.tideways_flags & TIDEWAYS_FLAGS_NO_SPANS) == 0) {
			hp_globals.cpu_start = cpu_timer();
		}

		tw_span_create("app", 3);
		tw_span_timer_start(0);

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

	tw_span_timer_stop(0);

	if ((hp_globals.tideways_flags & TIDEWAYS_FLAGS_NO_SPANS) == 0) {
		if ((GC_G(gc_runs) - hp_globals.gc_runs) > 0) {
			tw_span_annotate_long(0, "gc", GC_G(gc_runs) - hp_globals.gc_runs);
			tw_span_annotate_long(0, "gcc", GC_G(collected) - hp_globals.gc_collected);
		}

		if (hp_globals.compile_count > 0) {
			tw_span_annotate_long(0, "cct", hp_globals.compile_count);
		}
		if (hp_globals.compile_wt > 0) {
			tw_span_annotate_long(0, "cwt", hp_globals.compile_wt);
		}

		tw_span_annotate_long(0, "cpu", get_us_from_tsc(cpu_timer() - hp_globals.cpu_start));
	}

	if (hp_globals.root) {
		efree(hp_globals.root);
		hp_globals.root = NULL;
	}

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
static zval *hp_zval_at_key(char *key, size_t size, zval *values)
{
	if (Z_TYPE_P(values) == IS_ARRAY) {
		HashTable *ht = Z_ARRVAL_P(values);

		return zend_compat_hash_find_const(ht, key, size-1);
	}

	return NULL;
}

/**
 *  Convert the PHP array of strings to an emalloced array of strings. Note,
 *  this method duplicates the string data in the PHP array.
 *
 *  @author mpal
 **/
static char **hp_strings_in_zval(zval *values)
{
	char   **result;
	size_t   count;
	size_t   ix = 0;
	char  *str;
	uint   len;
	ulong  idx;
	int    type;
	zval **data, *val;

	if (!values) {
		return NULL;
	}

	if (Z_TYPE_P(values) == IS_ARRAY) {
		HashTable *ht;

		ht    = Z_ARRVAL_P(values);
		count = zend_hash_num_elements(ht);

		if((result =
					(char**)emalloc(sizeof(char*) * (count + 1))) == NULL) {
			return result;
		}

#if PHP_VERSION_ID < 70000
		for (zend_hash_internal_pointer_reset(ht);
				zend_hash_has_more_elements(ht) == SUCCESS;
				zend_hash_move_forward(ht)) {

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
#else
		ZEND_HASH_FOREACH_KEY_VAL(ht, idx, str, val) {
			if (str) {
				result[ix] = estrdup(str);
			} else {
				result[ix] = estrdup(Z_STRVAL_P(val));
			}
			ix++;
		} ZEND_HASH_FOREACH_END();
#endif

	} else if(Z_TYPE_P(values) == IS_STRING) {
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

	if (hp_globals.enabled) {
#if (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION >= 3) || PHP_MAJOR_VERSION >= 6
		error_handling  = EG(error_handling);
#else
		error_handling  = PG(error_handling);
#endif

		if (error_handling == EH_NORMAL) {
			switch (type) {
				case E_ERROR:
				case E_CORE_ERROR:
					backtrace = ecalloc(sizeof(zval), 1);

#if PHP_VERSION_ID <= 50399
					zend_fetch_debug_backtrace(backtrace, 1, 0 TSRMLS_CC);
#else
					zend_fetch_debug_backtrace(backtrace, 1, 0, 0 TSRMLS_CC);
#endif

					hp_globals.backtrace = backtrace;
			}
		}
	}

	tideways_original_error_cb(type, error_filename, error_lineno, format, args);
}

PHP_FUNCTION(tideways_span_watch)
{
	char *func = NULL, *category = NULL;
	strsize_t func_len, category_len;
	tw_trace_callback cb;

	if (!hp_globals.enabled || (hp_globals.tideways_flags & TIDEWAYS_FLAGS_NO_SPANS) > 0) {
		return;
	}

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|s!", &func, &func_len, &category, &category_len) == FAILURE) {
		return;
	}

	if (category != NULL && strcmp(category, "view") == 0) {
		cb = tw_trace_callback_view_engine;
	} else if (category != NULL && strcmp(category, "event") == 0) {
		cb = tw_trace_callback_event_dispatchers;
	} else {
		cb = tw_trace_callback_php_call;
	}

	register_trace_callback_len(func, func_len, cb);
}

static void free_tw_watch_callback(void *twcb)
{
	tw_watch_callback *_twcb = *((tw_watch_callback **)twcb);

#if PHP_VERSION_ID < 70000
	if (_twcb->fci.function_name) {
		zval_ptr_dtor((zval **)&_twcb->fci.function_name);
	}
	if (_twcb->fci.object_ptr) {
		zval_ptr_dtor((zval **)&_twcb->fci.object_ptr);
	}
#else
	// TODO
#endif

	efree(_twcb);
}

static void tideways_add_callback_watch(zend_fcall_info fci, zend_fcall_info_cache fcic, char *func, int func_len TSRMLS_DC)
{
	tw_watch_callback *twcb;
	tw_trace_callback cb;

	twcb = emalloc(sizeof(tw_watch_callback));
	twcb->fci = fci;
	twcb->fcic = fcic;

	if (hp_globals.trace_watch_callbacks == NULL) {
		ALLOC_HASHTABLE(hp_globals.trace_watch_callbacks);
		zend_hash_init(hp_globals.trace_watch_callbacks, 255, NULL, free_tw_watch_callback, 0);
	}

	zend_compat_hash_update_ptr_const(hp_globals.trace_watch_callbacks, func, func_len, &twcb, sizeof(tw_watch_callback*));
	cb = tw_trace_callback_watch;
	register_trace_callback_len(func, func_len, cb);
}

PHP_FUNCTION(tideways_span_callback)
{
	zend_fcall_info fci = empty_fcall_info;
	zend_fcall_info_cache fcic = empty_fcall_info_cache;
	char *func;
	strsize_t func_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sf", &func, &func_len, &fci, &fcic) == FAILURE) {
		zend_error(E_ERROR, "tideways_callback_watch() expects a string as a first and a callback as a second argument");
		return;
	}

#if PHP_VERSION_ID < 70000
	if (fci.size) {
		Z_ADDREF_P(fci.function_name);
		if (fci.object_ptr) {
			Z_ADDREF_P(fci.object_ptr);
		}
	}
#endif

	tideways_add_callback_watch(fci, fcic, func, func_len TSRMLS_CC);
}
