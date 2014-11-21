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
#ifdef PHP_QAFOOPROFILER_HAVE_CURL
#include <curl/curl.h>
#include <curl/easy.h>
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "ext/standard/file.h"
#include "php_qafooprofiler.h"
#include "zend_extensions.h"

#include "ext/pcre/php_pcre.h"
#include "ext/standard/url.h"
#include "ext/pdo/php_pdo_driver.h"
#include "zend_exceptions.h"
#include "zend_stream.h"

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

/* Qafoo Profiler version                           */
#define QAFOOPROFILER_VERSION       "1.2.2"

/* Fictitious function name to represent top of the call tree. The paranthesis
 * in the name is to ensure we don't conflict with user function names.  */
#define ROOT_SYMBOL                "main()"

/* Size of a temp scratch buffer            */
#define SCRATCH_BUF_LEN            512

/* Various QAFOOPROFILER modes. If you are adding a new mode, register the appropriate
 * callbacks in hp_begin() */
#define QAFOOPROFILER_MODE_HIERARCHICAL	1
#define QAFOOPROFILER_MODE_SAMPLED			620002      /* Rockfort's zip code */
#define QAFOOPROFILER_MODE_LAYER			2

/* Hierarchical profiling flags.
 *
 * Note: Function call counts and wall (elapsed) time are always profiled.
 * The following optional flags can be used to control other aspects of
 * profiling.
 */
#define QAFOOPROFILER_FLAGS_NO_BUILTINS   0x0001 /* do not profile builtins */
#define QAFOOPROFILER_FLAGS_CPU           0x0002 /* gather CPU times for funcs */
#define QAFOOPROFILER_FLAGS_MEMORY        0x0004 /* gather memory usage for funcs */
#define QAFOOPROFILER_FLAGS_NO_USERLAND   0x0008 /* do not profile userland functions */

/* Constants for QAFOOPROFILER_MODE_SAMPLED        */
#define QAFOOPROFILER_SAMPLING_INTERVAL       100000      /* In microsecs        */

/* Constant for ignoring functions, transparent to hierarchical profile */
#define QAFOOPROFILER_MAX_FILTERED_FUNCTIONS  256
#define QAFOOPROFILER_FILTERED_FUNCTION_SIZE                           \
               ((QAFOOPROFILER_MAX_FILTERED_FUNCTIONS + 7)/8)
#define QAFOOPROFILER_MAX_ARGUMENT_LEN 256

#if !defined(uint64)
typedef unsigned long long uint64;
#endif
#if !defined(uint32)
typedef unsigned int uint32;
#endif
#if !defined(uint8)
typedef unsigned char uint8;
#endif


/**
 * *****************************
 * GLOBAL DATATYPES AND TYPEDEFS
 * *****************************
 */

/* Qafoo Profiler maintains a stack of entries being profiled. The memory for the entry
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
} hp_entry_t;

/* Various types for QAFOOPROFILER callbacks       */
typedef void (*hp_init_cb)           (TSRMLS_D);
typedef void (*hp_exit_cb)           (TSRMLS_D);
typedef void (*hp_begin_function_cb) (hp_entry_t **entries,
                                      hp_entry_t *current   TSRMLS_DC);
typedef void (*hp_end_function_cb)   (hp_entry_t **entries  TSRMLS_DC);

/* Struct to hold the various callbacks for a single profiling mode */
typedef struct hp_mode_cb {
	hp_init_cb             init_cb;
	hp_exit_cb             exit_cb;
	hp_begin_function_cb   begin_fn_cb;
	hp_end_function_cb     end_fn_cb;
} hp_mode_cb;

typedef struct hp_string {
	const char *value;
	size_t length;
} hp_string;

/* Struct that defines caught errors or exceptions inside the engine. */
typedef struct hp_error {
	unsigned int line;
	hp_string *file;
	hp_string *message;
	hp_string *trace;
	unsigned int type;
	long code;
	hp_string *class;
} hp_error;

typedef struct hp_function_map {
	char **names;
	uint8 filter[QAFOOPROFILER_FILTERED_FUNCTION_SIZE];
} hp_function_map;

/* Qafoo Profiler's global state.
 *
 * This structure is instantiated once.  Initialize defaults for attributes in
 * hp_init_profiler_state() Cleanup/free attributes in
 * hp_clean_profiler_state() */
typedef struct hp_global_t {

	/*       ----------   Global attributes:  -----------       */

	/* Indicates if Qafoo Profiler is currently enabled */
	int              enabled;

	/* Indicates if Qafoo Profiler was ever enabled during this request */
	int              ever_enabled;

	/* Holds all information about layer profiling */
	zval            *layers_count;

	/* A key=>value list of function calls to their respective layers. */
	HashTable       *layers_definition;

	/* Holds all the Qafoo Profiler statistics */
	zval            *stats_count;

	/* Holds all the information about last error. */
	hp_error            *last_error;

	/* Holds the last exception */
	hp_error            *last_exception;

	/* Indicates the current Qafoo Profiler mode or level */
	int              profiler_level;

	/* Top of the profile stack */
	hp_entry_t      *entries;

	/* freelist of hp_entry_t chunks for reuse... */
	hp_entry_t      *entry_free_list;

	/* Callbacks for various Qafoo Profiler modes */
	hp_mode_cb      mode_cb;

	/* Function that determines the transaction name and callback */
	hp_string       *transaction_function;
	hp_string		*transaction_name;

	/*       ----------   Mode specific attributes:  -----------       */

	/* Global to track the time of the last sample in time and ticks */
	struct timeval   last_sample_time;
	uint64           last_sample_tsc;
	/* QAFOOPROFILER_SAMPLING_INTERVAL in ticks */
	uint64           sampling_interval_tsc;

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

	/* Qafoo Profiler flags */
	uint32 qafooprofiler_flags;

	/* counter table indexed by hash value of function names. */
	uint8  func_hash_counters[256];

	/* Table of filtered function names and their filter */
	int     filtered_type; // 1 = blacklist, 2 = whitelist, 0 = nothing

	hp_function_map *filtered_functions;
	hp_function_map *argument_functions;
	hp_function_map *trace_functions;

	/* Table of functions which allow custom tracing */
	char **trace_function_names;
	uint8   trace_function_filter[QAFOOPROFILER_FILTERED_FUNCTION_SIZE];

} hp_global_t;

#ifdef PHP_QAFOOPROFILER_HAVE_CURL
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

/**
 * ***********************
 * GLOBAL STATIC VARIABLES
 * ***********************
 */
/* Qafoo Profiler global state */
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
void (*qafooprofiler_original_error_cb)(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args);
void qafooprofiler_error_cb(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args);
static void qafooprofiler_throw_exception_hook(zval *exception TSRMLS_DC);

/* Bloom filter for function names to be ignored */
#define INDEX_2_BYTE(index)  (index >> 3)
#define INDEX_2_BIT(index)   (1 << (index & 0x7));


/**
 * ****************************
 * STATIC FUNCTION DECLARATIONS
 * ****************************
 */
static void hp_register_constants(INIT_FUNC_ARGS);

static void hp_begin(long level, long qafooprofiler_flags TSRMLS_DC);
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

static void hp_parse_options_from_arg(zval *args);
static void hp_parse_layers_options_from_arg(zval *layers);
static void hp_clean_profiler_options_state();

static void hp_transaction_function_clear();
static void hp_transaction_name_clear();

static inline zval  *hp_zval_at_key(char  *key, zval  *values);
static inline char **hp_strings_in_zval(zval  *values);
static inline void   hp_array_del(char **name_array);
static inline int  hp_argument_entry(uint8 hash_code, char *curr_func);
static inline hp_string *hp_create_string(const char *value, size_t length);
static inline long hp_zval_to_long(zval *z);
static inline hp_string *hp_zval_to_string(zval *z);
static inline zval *hp_string_to_zval(hp_string *str);
static inline void hp_string_clean(hp_string *str);

static inline hp_function_map *hp_function_map_create(char **names);
static inline void hp_function_map_clear(hp_function_map *map);
static inline int hp_function_map_exists(hp_function_map *map, uint8 hash_code, char *curr_func);
static inline int hp_function_map_filter_collision(hp_function_map *map, uint8 hash);

static hp_string *qafooprofiler_backtrace(TSRMLS_D);
static void hp_error_clean(hp_error *error);
static void hp_error_to_zval(hp_error *error, zval *z);
static hp_error *hp_error_create();

/* {{{ arginfo */
ZEND_BEGIN_ARG_INFO_EX(arginfo_qafooprofiler_enable, 0, 0, 0)
  ZEND_ARG_INFO(0, flags)
  ZEND_ARG_INFO(0, options)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_qafooprofiler_disable, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_qafooprofiler_transaction_name, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_qafooprofiler_last_fatal_error, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_qafooprofiler_last_exception_data, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_qafooprofiler_sample_enable, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_qafooprofiler_layers_enable, 0)
	ZEND_ARG_INFO(0, layers)
	ZEND_ARG_INFO(0, transaction_function)
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
/* List of functions implemented/exposed by Qafoo Profiler */
zend_function_entry qafooprofiler_functions[] = {
	PHP_FE(qafooprofiler_enable, arginfo_qafooprofiler_enable)
	PHP_FE(qafooprofiler_disable, arginfo_qafooprofiler_disable)
	PHP_FE(qafooprofiler_transaction_name, arginfo_qafooprofiler_transaction_name)
	PHP_FE(qafooprofiler_last_fatal_error, arginfo_qafooprofiler_last_fatal_error)
	PHP_FE(qafooprofiler_last_exception_data, arginfo_qafooprofiler_last_exception_data)
	PHP_FE(qafooprofiler_sample_enable, arginfo_qafooprofiler_sample_enable)
	PHP_FE(qafooprofiler_layers_enable, arginfo_qafooprofiler_layers_enable)
	{NULL, NULL, NULL}
};

/* Callback functions for the Qafoo Profiler extension */
zend_module_entry qafooprofiler_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"qafooprofiler",                        /* Name of the extension */
	qafooprofiler_functions,                /* List of functions exposed */
	PHP_MINIT(qafooprofiler),               /* Module init callback */
	PHP_MSHUTDOWN(qafooprofiler),           /* Module shutdown callback */
	PHP_RINIT(qafooprofiler),               /* Request init callback */
	PHP_RSHUTDOWN(qafooprofiler),           /* Request shutdown callback */
	PHP_MINFO(qafooprofiler),               /* Module info callback */
#if ZEND_MODULE_API_NO >= 20010901
	QAFOOPROFILER_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};

PHP_INI_BEGIN()

/**
 * INI-Settings are not yet used by the extension, but by the PHP library.
 */
PHP_INI_ENTRY("qafooprofiler.socket", "/tmp/qprofd.sock", PHP_INI_ALL, NULL)
PHP_INI_ENTRY("qafooprofiler.api_key", "", PHP_INI_ALL, NULL)
PHP_INI_ENTRY("qafooprofiler.transaction_function", "", PHP_INI_ALL, NULL)
PHP_INI_ENTRY("qafooprofiler.sample_rate", "10", PHP_INI_ALL, NULL)
PHP_INI_ENTRY("qafooprofiler.load_library", "1", PHP_INI_ALL, NULL)

PHP_INI_END()

/* Init module */
ZEND_GET_MODULE(qafooprofiler)


/**
 * **********************************
 * PHP EXTENSION FUNCTION DEFINITIONS
 * **********************************
 */

/**
 * Start Qafoo Profiler profiling in hierarchical mode.
 *
 * @param  long $flags  flags for hierarchical mode
 * @return void
 * @author kannan
 */
PHP_FUNCTION(qafooprofiler_enable)
{
	long  qafooprofiler_flags = 0;       /* Qafoo PRofiler flags */
	zval *optional_array = NULL;         /* optional array arg: for future use */

	if (hp_globals.enabled) {
		return;
	}

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
				"|lz", &qafooprofiler_flags, &optional_array) == FAILURE) {
		return;
	}

	hp_parse_options_from_arg(optional_array);

	hp_begin(QAFOOPROFILER_MODE_HIERARCHICAL, qafooprofiler_flags TSRMLS_CC);
}

PHP_FUNCTION(qafooprofiler_last_fatal_error)
{
	if (hp_globals.enabled && hp_globals.last_error) {
		hp_error_to_zval(hp_globals.last_error, return_value);
	}
}

PHP_FUNCTION(qafooprofiler_last_exception_data)
{
	if (hp_globals.enabled && hp_globals.last_exception) {
		hp_error_to_zval(hp_globals.last_exception, return_value);
	}
}

/**
 * Start Qafoo Profiler in sampling mode.
 *
 * @return void
 * @author cjiang
 */
PHP_FUNCTION(qafooprofiler_sample_enable)
{
	long  qafooprofiler_flags = 0;                                    /* Qafoo Profiler flags */
	hp_parse_options_from_arg(NULL);
	hp_begin(QAFOOPROFILER_MODE_SAMPLED, qafooprofiler_flags TSRMLS_CC);
}

/**
 * Start Qafoo Profiler profiling in layers mode.
 */
PHP_FUNCTION(qafooprofiler_layers_enable)
{
	long qafooprofiler_flags = QAFOOPROFILER_FLAGS_NO_USERLAND;

	zval *layers, *transaction_function = NULL;

	if (hp_globals.enabled) {
		return;
	}

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|zz", &layers, &transaction_function) == FAILURE) {
		return;
	}

	if (layers == NULL || Z_TYPE_P(layers) != IS_ARRAY) {
		zend_error(E_NOTICE, "qafooprofiler_layers_enable() requires first argument to be array");
		return;
	}

	if (transaction_function != NULL && Z_TYPE_P(transaction_function) == IS_STRING) {
		hp_globals.transaction_function = hp_zval_to_string(transaction_function);
	}

	if (zend_hash_num_elements(Z_ARRVAL_P(layers)) == 0) {
		qafooprofiler_flags = QAFOOPROFILER_FLAGS_NO_USERLAND | QAFOOPROFILER_FLAGS_NO_BUILTINS;
	}

	hp_clean_profiler_options_state();

	hp_parse_layers_options_from_arg(layers);

	hp_globals.filtered_type = 2;
	hp_globals.filtered_functions = hp_function_map_create(hp_strings_in_zval(layers));

	hp_begin(QAFOOPROFILER_MODE_LAYER, qafooprofiler_flags TSRMLS_CC);
}

/**
 * Stops Qafoo Profiler from profiling  and returns the profile info.
 *
 * @param  void
 * @return array  hash-array of Qafoo Profiler's profile info
 * @author cjiang
 */
PHP_FUNCTION(qafooprofiler_disable)
{
	zval *tmp, *value;
	void *data;

	if (!hp_globals.enabled) {
		return;
	}

	hp_stop(TSRMLS_C);

	if (hp_globals.profiler_level == QAFOOPROFILER_MODE_LAYER) {
		if (zend_hash_find(Z_ARRVAL_P(hp_globals.stats_count), ROOT_SYMBOL, strlen(ROOT_SYMBOL) + 1, &data) == SUCCESS) {
			tmp = *(zval **) data;

			add_assoc_zval(hp_globals.layers_count, "main()", tmp);
		}

		RETURN_ZVAL(hp_globals.layers_count, 1, 0);
	}

	if (hp_globals.profiler_level == QAFOOPROFILER_MODE_HIERARCHICAL) {
		if (hp_globals.layers_count) {
			if (zend_hash_find(Z_ARRVAL_P(hp_globals.stats_count), ROOT_SYMBOL, strlen(ROOT_SYMBOL) + 1, &data) == SUCCESS) {
				tmp = *(zval **) data;

				MAKE_STD_ZVAL(value);
				ZVAL_ZVAL(value, hp_globals.layers_count, 1, 0);
				add_assoc_zval(tmp, "layers", value);
			}
		}

		RETURN_ZVAL(hp_globals.stats_count, 1, 0);
	}

	if (hp_globals.profiler_level == QAFOOPROFILER_MODE_SAMPLED) {
		RETURN_ZVAL(hp_globals.stats_count, 1, 0);
	}
}

PHP_FUNCTION(qafooprofiler_transaction_name)
{
	if (hp_globals.transaction_name) {
		zval *ret = hp_string_to_zval(hp_globals.transaction_name);
		RETURN_ZVAL(ret, 1, 1);
	}
}

/**
 * Module init callback.
 *
 * @author cjiang
 */
PHP_MINIT_FUNCTION(qafooprofiler)
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
	hp_globals.last_error = NULL;
	hp_globals.last_exception = NULL;

	/* no free hp_entry_t structures to start with */
	hp_globals.entry_free_list = NULL;

	for (i = 0; i < 256; i++) {
		hp_globals.func_hash_counters[i] = 0;
	}

	hp_transaction_function_clear();

#if defined(DEBUG)
	/* To make it random number generator repeatable to ease testing. */
	srand(0);
#endif
	return SUCCESS;
}

/**
 * Module shutdown callback.
 */
PHP_MSHUTDOWN_FUNCTION(qafooprofiler)
{
	/* Make sure cpu_frequencies is free'ed. */
	clear_frequencies();

	/* free any remaining items in the free list */
	hp_free_the_free_list();

	UNREGISTER_INI_ENTRIES();

	return SUCCESS;
}

/**
 * Request init callback.
 *
 * Check if QafooProfiler.php exists in extension_dir and load it
 * in request init. This makes class \QafooLabs\Profiler available
 * for usage.
 */
PHP_RINIT_FUNCTION(qafooprofiler)
{
	if (INI_INT("qafooprofiler.load_library") == 0) {
		return SUCCESS;
	}

	zend_file_handle file_handle;
	zend_op_array *new_op_array;
	zval *result = NULL;
	int ret;
	int dummy = 1;
	char *extension_dir = INI_STR("extension_dir");
	char *profiler_file;
	int profiler_file_len;

	profiler_file_len = strlen(extension_dir) + strlen("QafooProfiler.php") + 2;
	profiler_file = emalloc(profiler_file_len);
	snprintf(profiler_file, profiler_file_len, "%s/%s", extension_dir, "QafooProfiler.php");

	ret = php_stream_open_for_zend_ex(profiler_file, &file_handle, STREAM_OPEN_FOR_INCLUDE TSRMLS_CC);

	// This code is copied from spl_autoload function in ext/spl/php_spl.c - there was no way to trigger
	// it, because it would rely on the include path and we can't know where that is during
	// installation. Putting the file into the extension_dir makes it much easier.
	if (ret == SUCCESS) {
		if (!file_handle.opened_path) {
			file_handle.opened_path = profiler_file;
		}
		if (zend_hash_add(&EG(included_files), file_handle.opened_path, strlen(file_handle.opened_path)+1, (void *)&dummy, sizeof(int), NULL)==SUCCESS) {
			new_op_array = zend_compile_file(&file_handle, ZEND_REQUIRE TSRMLS_CC);
			zend_destroy_file_handle(&file_handle TSRMLS_CC);
		} else {
			new_op_array = NULL;
			zend_file_handle_dtor(&file_handle TSRMLS_CC);
		}
		if (new_op_array) {
			EG(return_value_ptr_ptr) = &result;
			EG(active_op_array) = new_op_array;
			if (!EG(active_symbol_table)) {
				zend_rebuild_symbol_table(TSRMLS_C);
			}

			zend_execute(new_op_array TSRMLS_CC);

			destroy_op_array(new_op_array TSRMLS_CC);
			efree(new_op_array);
			if (!EG(exception)) {
				if (EG(return_value_ptr_ptr)) {
					zval_ptr_dtor(EG(return_value_ptr_ptr));
				}
			}
		}
	}

	efree(profiler_file);

	return SUCCESS;
}

/**
 * Request shutdown callback. Stop profiling and return.
 */
PHP_RSHUTDOWN_FUNCTION(qafooprofiler)
{
	hp_end(TSRMLS_C);
	return SUCCESS;
}

/**
 * Module info callback. Returns the Qafoo Profiler version.
 */
PHP_MINFO_FUNCTION(qafooprofiler)
{
	char buf[SCRATCH_BUF_LEN];
	char tmp[SCRATCH_BUF_LEN];
	int i;
	int len;

	php_info_print_table_start();
	php_info_print_table_header(2, "qafooprofiler", QAFOOPROFILER_VERSION);
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


	php_info_print_table_end();
}


/**
 * ***************************************************
 * COMMON HELPER FUNCTION DEFINITIONS AND LOCAL MACROS
 * ***************************************************
 */

static void hp_register_constants(INIT_FUNC_ARGS)
{
	REGISTER_LONG_CONSTANT("QAFOOPROFILER_FLAGS_NO_BUILTINS",
			QAFOOPROFILER_FLAGS_NO_BUILTINS,
			CONST_CS | CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("QAFOOPROFILER_FLAGS_CPU",
			QAFOOPROFILER_FLAGS_CPU,
			CONST_CS | CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("QAFOOPROFILER_FLAGS_MEMORY",
			QAFOOPROFILER_FLAGS_MEMORY,
			CONST_CS | CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("QAFOOPROFILER_FLAGS_NO_USERLAND",
			QAFOOPROFILER_FLAGS_NO_USERLAND,
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
static inline uint8 hp_inline_hash(char * str)
{
	ulong h = 5381;
	uint i = 0;
	uint8 res = 0;

	while (*str) {
		h += (h << 5);
		h ^= (ulong) *str++;
	}

	for (i = 0; i < sizeof(ulong); i++) {
		res += ((uint8 *)&h)[i];
	}
	return res;
}

static void hp_parse_layers_options_from_arg(zval *layers)
{
	if (layers == NULL || Z_TYPE_P(layers) != IS_ARRAY) {
		return;
	}

	HashTable *ht;
	zval *tmp;

	ALLOC_HASHTABLE(hp_globals.layers_definition);
	zend_hash_init(hp_globals.layers_definition, 0, NULL, ZVAL_PTR_DTOR, 0);
	zend_hash_copy(
		hp_globals.layers_definition,
		Z_ARRVAL_P(layers),
		(copy_ctor_func_t)zval_add_ref,
		(void*)&tmp,
		sizeof(zval *)
	);
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

	zresult = hp_zval_at_key("argument_functions", args);
	hp_globals.argument_functions = hp_function_map_create(hp_strings_in_zval(zresult));

	zresult = hp_zval_at_key("layers", args);
	hp_parse_layers_options_from_arg(zresult);

	zresult = hp_zval_at_key("transaction_function", args);

	if (zresult != NULL) {
		hp_globals.transaction_function = hp_zval_to_string(zresult);
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

	memset(map->filter, 0, QAFOOPROFILER_FILTERED_FUNCTION_SIZE);

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

	memset(map->filter, 0, QAFOOPROFILER_FILTERED_FUNCTION_SIZE);
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
void hp_init_profiler_state(int level TSRMLS_DC)
{
	/* Setup globals */
	if (!hp_globals.ever_enabled) {
		hp_globals.ever_enabled  = 1;
		hp_globals.entries = NULL;
	}
	hp_globals.profiler_level  = (int) level;

	/* Init stats_count */
	if (hp_globals.stats_count) {
		zval_dtor(hp_globals.stats_count);
		FREE_ZVAL(hp_globals.stats_count);
	}
	MAKE_STD_ZVAL(hp_globals.stats_count);
	array_init(hp_globals.stats_count);

	if (hp_globals.layers_count) {
		zval_dtor(hp_globals.layers_count);
		FREE_ZVAL(hp_globals.layers_count);
		hp_globals.layers_count = NULL;
	}

	if (hp_globals.layers_definition) {
		MAKE_STD_ZVAL(hp_globals.layers_count);
		array_init(hp_globals.layers_count);
	}

	if (hp_globals.last_error) {
		hp_error_clean(hp_globals.last_error);
		hp_globals.last_error = NULL;
	}

	if (hp_globals.last_exception) {
		hp_error_clean(hp_globals.last_exception);
		hp_globals.last_exception = NULL;
	}

	/* NOTE(cjiang): some fields such as cpu_frequencies take relatively longer
	 * to initialize, (5 milisecond per logical cpu right now), therefore we
	 * calculate them lazily. */
	if (hp_globals.cpu_frequencies == NULL) {
		get_all_cpu_frequencies();
		restore_cpu_affinity(&hp_globals.prev_mask);
	}

	/* bind to a random cpu so that we can use rdtsc instruction. */
	bind_to_cpu((int) (rand() % hp_globals.cpu_num));

	/* Call current mode's init cb */
	hp_globals.mode_cb.init_cb(TSRMLS_C);

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
	/* Call current mode's exit cb */
	hp_globals.mode_cb.exit_cb(TSRMLS_C);

	/* Clear globals */
	if (hp_globals.stats_count) {
		zval_dtor(hp_globals.stats_count);
		FREE_ZVAL(hp_globals.stats_count);
		hp_globals.stats_count = NULL;
	}

	if (hp_globals.layers_count) {
		zval_dtor(hp_globals.layers_count);
		FREE_ZVAL(hp_globals.layers_count);
		hp_globals.layers_count = NULL;
	}

	if (hp_globals.last_error) {
		hp_error_clean(hp_globals.last_error);
		hp_globals.last_error = NULL;
	}

	if (hp_globals.last_exception) {
		hp_error_clean(hp_globals.last_exception);
		hp_globals.last_exception = NULL;
	}

	hp_globals.entries = NULL;
	hp_globals.profiler_level = 1;
	hp_globals.ever_enabled = 0;

	hp_clean_profiler_options_state();
	hp_transaction_function_clear();
	hp_transaction_name_clear();

	hp_function_map_clear(hp_globals.filtered_functions);
	hp_globals.filtered_functions = NULL;
	hp_function_map_clear(hp_globals.argument_functions);
	hp_globals.argument_functions = NULL;
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
	if (hp_globals.layers_definition) {
		zend_hash_destroy(hp_globals.layers_definition);
		FREE_HASHTABLE(hp_globals.layers_definition);
		hp_globals.layers_definition = NULL;
	}

	hp_function_map_clear(hp_globals.filtered_functions);
	hp_globals.filtered_functions = NULL;
	hp_function_map_clear(hp_globals.argument_functions);
	hp_globals.argument_functions = NULL;
}

/*
 * Start profiling - called just before calling the actual function
 * NOTE:  PLEASE MAKE SURE TSRMLS_CC IS AVAILABLE IN THE CONTEXT
 *        OF THE FUNCTION WHERE THIS MACRO IS CALLED.
 *        TSRMLS_CC CAN BE MADE AVAILABLE VIA TSRMLS_DC IN THE
 *        CALLING FUNCTION OR BY CALLING TSRMLS_FETCH()
 *        TSRMLS_FETCH() IS RELATIVELY EXPENSIVE.
 */
#define BEGIN_PROFILING(entries, symbol, profile_curr)							\
	do {																		\
		/* Use a hash code to filter most of the string comparisons. */			\
		uint8 hash_code  = hp_inline_hash(symbol);								\
		profile_curr = !hp_filter_entry(hash_code, symbol);						\
		if (profile_curr) {														\
			hp_entry_t *cur_entry = hp_fast_alloc_hprof_entry();				\
			(cur_entry)->hash_code = hash_code;									\
			(cur_entry)->name_hprof = symbol;									\
			(cur_entry)->prev_hprof = (*(entries));								\
			/* Call the universal callback */									\
			hp_mode_common_beginfn((entries), (cur_entry) TSRMLS_CC);			\
			/* Call the mode's beginfn callback */								\
			hp_globals.mode_cb.begin_fn_cb((entries), (cur_entry) TSRMLS_CC);   \
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
#define END_PROFILING(entries, profile_curr)								\
	do {																	\
		if (profile_curr) {													\
			hp_entry_t *cur_entry;											\
			/* Call the mode's endfn callback. */							\
			/* NOTE(cjiang): we want to call this 'end_fn_cb' before */		\
			/* 'hp_mode_common_endfn' to avoid including the time in */		\
			/* 'hp_mode_common_endfn' in the profiling results.      */		\
			hp_globals.mode_cb.end_fn_cb((entries) TSRMLS_CC);				\
			cur_entry = (*(entries));										\
			/* Call the universal callback */								\
			hp_mode_common_endfn((entries), (cur_entry) TSRMLS_CC);			\
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
size_t hp_get_entry_name(hp_entry_t  *entry,
		char           *result_buf,
		size_t          result_len) {

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
	}
	else {
		snprintf(
			result_buf,
			result_len,
			"%s",
			entry->name_hprof
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
size_t hp_get_function_stack(hp_entry_t *entry,
		int            level,
		char          *result_buf,
		size_t         result_len) {

	size_t         len = 0;

	/* End recursion if we dont need deeper levels or we dont have any deeper
	 * levels */
	if (!entry->prev_hprof || (level <= 1)) {
		return hp_get_entry_name(entry, result_buf, result_len);
	}

	/* Take care of all ancestors first */
	len = hp_get_function_stack(entry->prev_hprof,
			level - 1,
			result_buf,
			result_len);

	/* Append the delimiter */
# define    HP_STACK_DELIM        "==>"
# define    HP_STACK_DELIM_LEN    (sizeof(HP_STACK_DELIM) - 1)

	if (result_len < (len + HP_STACK_DELIM_LEN)) {
		/* Insufficient result_buf. Bail out! */
		return len;
	}

	/* Add delimiter only if entry had ancestors */
	if (len) {
		strncat(result_buf + len,
				HP_STACK_DELIM,
				result_len - len);
		len += HP_STACK_DELIM_LEN;
	}

# undef     HP_STACK_DELIM_LEN
# undef     HP_STACK_DELIM

	/* Append the current function name */
	return len + hp_get_entry_name(entry,
			result_buf + len,
			result_len - len);
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
	int array_count, result_len, found;
	char *result, *token;

	found = 0;
	result = "";
	MAKE_STD_ZVAL(parts);

	if ((pce = pcre_get_compiled_regex_cache("(([\\s]+))", 8 TSRMLS_CC)) == NULL) {
		return "";
	}

	php_pcre_split_impl(pce, sql, len, parts, -1, 0 TSRMLS_CC);

	arrayParts = Z_ARRVAL_P(parts);

	result_len = QAFOOPROFILER_MAX_ARGUMENT_LEN;
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
		} else if (strcmp(token, "from") == 0) {
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

	len = QAFOOPROFILER_MAX_ARGUMENT_LEN;
	ret = emalloc(len);
	snprintf(ret, len, "");

	url = php_url_parse_ex(filename, filename_len);

	if (url->scheme) {
		snprintf(ret, len, "%s%s://", ret, url->scheme);
	}

	if (url->host) {
		snprintf(ret, len, "%s%s", ret, url->host);
	}

	if (url->port) {
		snprintf(ret, len, "%s%d", ret, url->port);
	}

	if (url->path) {
		snprintf(ret, len, "%s%s", ret, url->path);
	}

	/*
	 * We assume the stream will be opened,
	 * pointing to the next free element in resource list.
	 *
	 * This does not reliably work however, the first opened stream
	 * (http,file) will open two entries, creating an offset. We can
	 * handle this in the profiler parsing for now.
	 */
	snprintf(ret, len, "%s#%d", ret, EG(regular_list).nNextFreeElement);

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

static char *hp_get_function_argument_summary(char *ret, int len, zend_execute_data *data TSRMLS_DC)
{
	void **p;
	int arg_count = 0;
	int i;
	zval *argument_element;
	/* oldret holding function name or class::function. We will reuse the string and free it after */
	char *oldret = ret;
	char *summary;

	p = hp_get_execute_arguments(data);
	arg_count = (int)(zend_uintptr_t) *p;       /* this is the amount of arguments passed to function */

	len = QAFOOPROFILER_MAX_ARGUMENT_LEN;
	ret = emalloc(len);
	snprintf(ret, len, "%s#", oldret);
	efree(oldret);

	if (strcmp(ret, "fgets#") == 0 ||
			strcmp(ret, "fgetcsv#") == 0 ||
			strcmp(ret, "fread#") == 0 ||
			strcmp(ret, "fwrite#") == 0 ||
			strcmp(ret, "fputs#") == 0 ||
			strcmp(ret, "fputcsv#") == 0 ||
			strcmp(ret, "stream_get_contents#") == 0 ||
			strcmp(ret, "fclose#") == 0
	   ) {

		php_stream *stream;

		argument_element = *(p-arg_count);

		if (Z_TYPE_P(argument_element) == IS_RESOURCE) {
			php_stream_from_zval_no_verify(stream, &argument_element);

			if (stream != NULL && stream->orig_path) {
				snprintf(ret, len, "%s%d", ret, stream->rsrc_id);
			}
		}
	} else if (strcmp(ret, "fopen#") == 0 || strcmp(ret, "file_get_contents#") == 0 || strcmp(ret, "file_put_contents#") == 0) {
		argument_element = *(p-arg_count);

		if (Z_TYPE_P(argument_element) == IS_STRING) {
			summary = hp_get_file_summary(Z_STRVAL_P(argument_element), Z_STRLEN_P(argument_element) TSRMLS_CC);

			snprintf(ret, len, "%s%s", ret, summary);

			efree(summary);
		}
#ifdef PHP_QAFOOPROFILER_HAVE_CURL
	} else if (strcmp(ret, "curl_exec#") == 0) {
		hp_curl_t *ch;
		int  le_curl;
		char *s_code;

		le_curl = zend_fetch_list_dtor_id("curl");

		argument_element = *(p-arg_count);

		if (Z_TYPE_P(argument_element) == IS_RESOURCE) {
			ZEND_FETCH_RESOURCE_NO_RETURN(ch, hp_curl_t *, &argument_element, -1, "cURL handle", le_curl);

			if (ch && curl_easy_getinfo(ch->cp, CURLINFO_EFFECTIVE_URL, &s_code) == CURLE_OK) {
				summary = hp_get_file_summary(s_code, strlen(s_code) TSRMLS_CC);
				snprintf(ret, len, "%s%s", ret, summary);
				efree(summary);
			}
		}
#endif
	} else if (strcmp(ret, "PDO::exec#") == 0 ||
			strcmp(ret, "PDO::query#") == 0 ||
			strcmp(ret, "mysql_query#") == 0 ||
			strcmp(ret, "mysqli_query#") == 0 ||
			strcmp(ret, "mysqli::query#") == 0) {

		if (strcmp(ret, "mysqli_query#") == 0) {
			argument_element = *(p-arg_count+1);
		} else {
			argument_element = *(p-arg_count);
		}

		if (Z_TYPE_P(argument_element) == IS_STRING) {
			summary = hp_get_sql_summary(Z_STRVAL_P(argument_element), Z_STRLEN_P(argument_element) TSRMLS_CC);

			snprintf(ret, len, "%s%s", ret, summary);
			efree(summary);
		}

	} else if (strcmp(ret, "PDOStatement::execute#") == 0) {
		pdo_stmt_t *stmt = (pdo_stmt_t*)zend_object_store_get_object_by_handle( (((*((*data).object)).value).obj).handle TSRMLS_CC);

		summary = hp_get_sql_summary(stmt->query_string, stmt->query_stringlen TSRMLS_CC);

		snprintf(ret, len, "%s%s", ret, summary);

		efree(summary);
	} else if (strcmp(ret, "Twig_Template::render#") == 0 || strcmp(ret, "Twig_Template::display#") == 0) {
		zval fname, *retval_ptr, *obj;

		ZVAL_STRING(&fname, "getTemplateName", 0);
		obj = data->object;

		if (SUCCESS == call_user_function_ex(EG(function_table), &obj, &fname, &retval_ptr, 0, NULL, 1, NULL TSRMLS_CC)) {
			snprintf(ret, len, "%s%s", ret, Z_STRVAL_P(retval_ptr));

			FREE_ZVAL(retval_ptr);
		}
	} else if (strcmp(ret, "Symfony\\Component\\EventDispatcher\\EventDispatcher::dispatch#") == 0 ||
			strcmp(ret, "Doctrine\\Common\\EventManager::dispatchEvent#") == 0 ||
			strcmp(ret, "Enlight\\Event\\EventManager::filter#") == 0 ||
			strcmp(ret, "Enlight\\Event\\EventManager::notify#") == 0 ||
			strcmp(ret, "Enlight\\Event\\EventManager::notifyUntil#") == 0 ||
			strcmp(ret, "Zend\\EventManager\\EventManager::trigger#") == 0) {
		argument_element = *(p-arg_count);

		if (argument_element && Z_TYPE_P(argument_element) == IS_STRING) {
			snprintf(ret, len, "%s%s", ret, Z_STRVAL_P(argument_element));
		}
	} else if (strcmp(ret, "Smarty::fetch#") == 0) {
		argument_element = *(p-arg_count);

		if (argument_element && Z_TYPE_P(argument_element) == IS_STRING) {
			snprintf(ret, len, "%s%s", ret, Z_STRVAL_P(argument_element));
		}
	} else if (strcmp(ret, "Smarty_Internal_TemplateBase::fetch#") == 0) {
		argument_element = *(p-arg_count);

		if (argument_element && Z_TYPE_P(argument_element) == IS_STRING) {
			snprintf(ret, len, "%s%s", ret, Z_STRVAL_P(argument_element));
		} else {
			zval *obj = data->object;
			zend_class_entry *smarty_ce = data->function_state.function->common.scope;

			argument_element = zend_read_property(smarty_ce, obj, "template_resource", sizeof("template_resource") - 1, 1 TSRMLS_CC);
			snprintf(ret, len, "%s%s", ret, Z_STRVAL_P(argument_element));
		}
	} else {
		for (i=0; i < arg_count; i++) {
			argument_element = *(p-(arg_count-i));

			switch(argument_element->type) {
				case IS_STRING:
					snprintf(ret, len, "%s%s", ret, Z_STRVAL_P(argument_element));
					break;

				case IS_LONG:
				case IS_BOOL:
					snprintf(ret, len, "%s%ld", ret, Z_LVAL_P(argument_element));
					break;

				case IS_DOUBLE:
					snprintf(ret, len, "%s%f", ret, Z_DVAL_P(argument_element));
					break;

				case IS_ARRAY:
					snprintf(ret, len, "%s%s", ret, "[...]");
					break;

				case IS_NULL:
					snprintf(ret, len, "%s%s", ret, "NULL");
					break;

				default:
					snprintf(ret, len, "%s%s", ret, "object");
			}

			if (i < arg_count-1) {
				snprintf(ret, len, "%s, ", ret);
			}
		}
	}

	return ret;
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
	uint8 hash_code;

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
				len = strlen(cls) + strlen(func) + 10;
				ret = (char*)emalloc(len);
				snprintf(ret, len, "%s::%s", cls, func);
			} else {
				ret = estrdup(func);
			}

			hash_code  = hp_inline_hash(ret);

			if (hp_argument_entry(hash_code, ret)) {
				ret = hp_get_function_argument_summary(ret, len, data TSRMLS_CC);
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
 * Sample the stack. Add it to the stats_count global.
 *
 * @param  tv            current time
 * @param  entries       func stack as linked list of hp_entry_t
 * @return void
 * @author veeve
 */
void hp_sample_stack(hp_entry_t  **entries  TSRMLS_DC)
{
	char key[SCRATCH_BUF_LEN];
	char symbol[SCRATCH_BUF_LEN * 1000];

	/* Build key */
	snprintf(
		key,
		sizeof(key),
		"%d.%06d",
		hp_globals.last_sample_time.tv_sec,
		hp_globals.last_sample_time.tv_usec
	);

	/* Init stats in the global stats_count hashtable */
	hp_get_function_stack(
		*entries,
		INT_MAX,
		symbol,
		sizeof(symbol)
	);

	add_assoc_string(
		hp_globals.stats_count,
		key,
		symbol,
		1
	);

	return;
}

/**
 * Checks to see if it is time to sample the stack.
 * Calls hp_sample_stack() if its time.
 *
 * @param  entries        func stack as linked list of hp_entry_t
 * @param  last_sample    time the last sample was taken
 * @param  sampling_intr  sampling interval in microsecs
 * @return void
 * @author veeve
 */
void hp_sample_check(hp_entry_t **entries  TSRMLS_DC)
{
	/* Validate input */
	if (!entries || !(*entries)) {
		return;
	}

	/* See if its time to sample.  While loop is to handle a single function
	 * taking a long time and passing several sampling intervals. */
	while ((cycle_timer() - hp_globals.last_sample_tsc)
			> hp_globals.sampling_interval_tsc) {

		/* bump last_sample_tsc */
		hp_globals.last_sample_tsc += hp_globals.sampling_interval_tsc;

		/* bump last_sample_time - HAS TO BE UPDATED BEFORE calling hp_sample_stack */
		incr_us_interval(&hp_globals.last_sample_time, QAFOOPROFILER_SAMPLING_INTERVAL);

		/* sample the stack */
		hp_sample_stack(entries  TSRMLS_CC);
	}

	return;
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
 * ***************************
 * QAFOOPROFILER DUMMY CALLBACKS
 * ***************************
 */
void hp_mode_dummy_init_cb(TSRMLS_D) { }


void hp_mode_dummy_exit_cb(TSRMLS_D) { }


void hp_mode_dummy_beginfn_cb(hp_entry_t **entries,
                              hp_entry_t *current  TSRMLS_DC) { }

void hp_mode_dummy_endfn_cb(hp_entry_t **entries   TSRMLS_DC) { }


/**
 * ****************************
 * QAFOOPROFILER COMMON CALLBACKS
 * ****************************
 */
/**
 * QAFOOPROFILER universal begin function.
 * This function is called for all modes before the
 * mode's specific begin_function callback is called.
 *
 * @param  hp_entry_t **entries  linked list (stack)
 *                                  of hprof entries
 * @param  hp_entry_t  *current  hprof entry for the current fn
 * @return void
 * @author kannan, veeve
 */
void hp_mode_common_beginfn(hp_entry_t **entries, hp_entry_t *current TSRMLS_DC)
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
}

/**
 * QAFOOPROFILER universal end function.  This function is called for all modes after
 * the mode's specific end_function callback is called.
 *
 * @param  hp_entry_t **entries  linked list (stack) of hprof entries
 * @return void
 * @author kannan, veeve
 */
void hp_mode_common_endfn(hp_entry_t **entries, hp_entry_t *current TSRMLS_DC)
{
	hp_globals.func_hash_counters[current->hash_code]--;
}


/**
 * *********************************
 * QAFOOPROFILER INIT MODULE CALLBACKS
 * *********************************
 */
/**
 * QAFOOPROFILER_MODE_SAMPLED's init callback
 *
 * @author veeve
 */
void hp_mode_sampled_init_cb(TSRMLS_D)
{
	struct timeval  now;
	uint64 truncated_us;
	uint64 truncated_tsc;
	double cpu_freq = hp_globals.cpu_frequencies[hp_globals.cur_cpu_id];

	/* Init the last_sample in tsc */
	hp_globals.last_sample_tsc = cycle_timer();

	/* Find the microseconds that need to be truncated */
	gettimeofday(&hp_globals.last_sample_time, 0);
	now = hp_globals.last_sample_time;
	hp_trunc_time(&hp_globals.last_sample_time, QAFOOPROFILER_SAMPLING_INTERVAL);

	/* Subtract truncated time from last_sample_tsc */
	truncated_us  = get_us_interval(&hp_globals.last_sample_time, &now);
	truncated_tsc = get_tsc_from_us(truncated_us, cpu_freq);
	if (hp_globals.last_sample_tsc > truncated_tsc) {
		/* just to be safe while subtracting unsigned ints */
		hp_globals.last_sample_tsc -= truncated_tsc;
	}

	/* Convert sampling interval to ticks */
	hp_globals.sampling_interval_tsc =
		get_tsc_from_us(QAFOOPROFILER_SAMPLING_INTERVAL, cpu_freq);
}


/**
 * ************************************
 * QAFOOPROFILER BEGIN FUNCTION CALLBACKS
 * ************************************
 */

/**
 * QAFOOPROFILER_MODE_HIERARCHICAL's begin function callback
 *
 * @author kannan
 */
void hp_mode_hier_beginfn_cb(hp_entry_t **entries, hp_entry_t *current TSRMLS_DC)
{
	/* Get start tsc counter */
	current->tsc_start = cycle_timer();

	/* Get CPU usage */
	if (hp_globals.qafooprofiler_flags & QAFOOPROFILER_FLAGS_CPU) {
		getrusage(RUSAGE_SELF, &(current->ru_start_hprof));
	}

	/* Get memory usage */
	if (hp_globals.qafooprofiler_flags & QAFOOPROFILER_FLAGS_MEMORY) {
		current->mu_start_hprof  = zend_memory_usage(0 TSRMLS_CC);
		current->pmu_start_hprof = zend_memory_peak_usage(0 TSRMLS_CC);
	}
}


/**
 * QAFOOPROFILER_MODE_SAMPLED's begin function callback
 *
 * @author veeve
 */
void hp_mode_sampled_beginfn_cb(hp_entry_t **entries, hp_entry_t *current TSRMLS_DC)
{
	/* See if its time to take a sample */
	hp_sample_check(entries  TSRMLS_CC);
}


/**
 * **********************************
 * QAFOOPROFILER END FUNCTION CALLBACKS
 * **********************************
 */

/**
 * QAFOOPROFILER shared end function callback
 *
 * @author kannan
 */
zval * hp_mode_shared_endfn_cb(hp_entry_t *top, char *symbol TSRMLS_DC)
{
	zval    *counts;
	uint64   tsc_end;
	double   wt;

	/* Get end tsc counter */
	tsc_end = cycle_timer();

	/* Get the stat array */
	if (!(counts = hp_hash_lookup(hp_globals.stats_count, symbol TSRMLS_CC))) {
		return (zval *) 0;
	}

	wt = get_us_from_tsc(tsc_end - top->tsc_start, hp_globals.cpu_frequencies[hp_globals.cur_cpu_id]);

	/* Bump stats in the counts hashtable */
	hp_inc_count(counts, "ct", 1  TSRMLS_CC);
	hp_inc_count(counts, "wt", wt TSRMLS_CC);

	if (hp_globals.layers_definition) {
		void **data;
		zval *layer, *layer_counts;
		char function_name[SCRATCH_BUF_LEN];

		hp_get_function_stack(top, 1, function_name, sizeof(function_name));

		if (zend_hash_find(hp_globals.layers_definition, function_name, strlen(function_name)+1, (void**)&data) == SUCCESS) {
			layer = *data;

			if (layer_counts = hp_hash_lookup(hp_globals.layers_count, Z_STRVAL_P(layer) TSRMLS_CC)) {
				hp_inc_count(layer_counts, "ct", 1  TSRMLS_CC);
				hp_inc_count(layer_counts, "wt", wt TSRMLS_CC);
			}
		}
	}

	return counts;
}

/**
 * QAFOOPROFILER_MODE_HIERARCHICAL's end function callback
 *
 * @author kannan
 */
void hp_mode_hier_endfn_cb(hp_entry_t **entries  TSRMLS_DC)
{
	hp_entry_t   *top = (*entries);
	zval            *counts;
	struct rusage    ru_end;
	char             symbol[SCRATCH_BUF_LEN];
	long int         mu_end;
	long int         pmu_end;

	/* Get the stat array */
	hp_get_function_stack(top, 2, symbol, sizeof(symbol));
	if (!(counts = hp_mode_shared_endfn_cb(top,
					symbol  TSRMLS_CC))) {
		return;
	}

	if (hp_globals.qafooprofiler_flags & QAFOOPROFILER_FLAGS_CPU) {
		/* Get CPU usage */
		getrusage(RUSAGE_SELF, &ru_end);

		/* Bump CPU stats in the counts hashtable */
		hp_inc_count(counts, "cpu", (get_us_interval(&(top->ru_start_hprof.ru_utime),
						&(ru_end.ru_utime)) +
					get_us_interval(&(top->ru_start_hprof.ru_stime),
						&(ru_end.ru_stime)))
				TSRMLS_CC);
	}

	if (hp_globals.qafooprofiler_flags & QAFOOPROFILER_FLAGS_MEMORY) {
		/* Get Memory usage */
		mu_end  = zend_memory_usage(0 TSRMLS_CC);
		pmu_end = zend_memory_peak_usage(0 TSRMLS_CC);

		/* Bump Memory stats in the counts hashtable */
		hp_inc_count(counts, "mu",  mu_end - top->mu_start_hprof    TSRMLS_CC);
		hp_inc_count(counts, "pmu", pmu_end - top->pmu_start_hprof  TSRMLS_CC);
	}
}

/**
 * Simplified layer end callback, to avoid unnecessary computations comapred to hierachical end callback.
 */
void hp_mode_layer_endfn_cb(hp_entry_t **entries  TSRMLS_DC)
{
	hp_entry_t   *top = (*entries);
	void **data;
	zval *layer, *layer_counts;
	char function_name[SCRATCH_BUF_LEN];
	uint64   tsc_end;
	double   wt;

	/* Get end tsc counter */
	tsc_end = cycle_timer();

	wt = get_us_from_tsc(tsc_end - top->tsc_start, hp_globals.cpu_frequencies[hp_globals.cur_cpu_id]);

	if (!hp_globals.layers_definition) {
		return;
	}

	hp_get_function_stack(top, 1, function_name, sizeof(function_name));

	if (zend_hash_find(hp_globals.layers_definition, function_name, strlen(function_name)+1, (void**)&data) == SUCCESS) {
		layer = *data;

		if (layer_counts = hp_hash_lookup(hp_globals.layers_count, Z_STRVAL_P(layer) TSRMLS_CC)) {
			hp_inc_count(layer_counts, "ct", 1  TSRMLS_CC);
			hp_inc_count(layer_counts, "wt", wt TSRMLS_CC);
		}
	}
}

/**
 * QAFOOPROFILER_MODE_SAMPLED's end function callback
 *
 * @author veeve
 */
void hp_mode_sampled_endfn_cb(hp_entry_t **entries  TSRMLS_DC)
{
	/* See if its time to take a sample */
	hp_sample_check(entries  TSRMLS_CC);
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
		efree(func);
	}

#if PHP_VERSION_ID < 50500
	_zend_execute(ops TSRMLS_CC);
#else
	_zend_execute_ex(execute_data TSRMLS_CC);
#endif

	if (hp_globals.transaction_name) {
#if PHP_VERSION_ID < 50500
		zend_execute          = _zend_execute;
#else
		zend_execute_ex       = _zend_execute_ex;
#endif
	}
}

/**
 * Qafoo Profiler enable replaced the zend_execute function with this
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

	BEGIN_PROFILING(&hp_globals.entries, func, hp_profile_flag);
#if PHP_VERSION_ID < 50500
	_zend_execute(ops TSRMLS_CC);
#else
	_zend_execute_ex(execute_data TSRMLS_CC);
#endif
	if (hp_globals.entries) {
		END_PROFILING(&hp_globals.entries, hp_profile_flag);
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
		BEGIN_PROFILING(&hp_globals.entries, func, hp_profile_flag);
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
			END_PROFILING(&hp_globals.entries, hp_profile_flag);
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

	BEGIN_PROFILING(&hp_globals.entries, func, hp_profile_flag);
	ret = _zend_compile_file(file_handle, type TSRMLS_CC);
	if (hp_globals.entries) {
		END_PROFILING(&hp_globals.entries, hp_profile_flag);
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

	filename = hp_get_base_filename(filename);
	len  = strlen("eval") + strlen(filename) + 3;
	func = (char *)emalloc(len);
	snprintf(func, len, "eval::%s", filename);

	BEGIN_PROFILING(&hp_globals.entries, func, hp_profile_flag);
	ret = _zend_compile_string(source_string, filename TSRMLS_CC);
	if (hp_globals.entries) {
		END_PROFILING(&hp_globals.entries, hp_profile_flag);
	}

	efree(func);
	return ret;
}

/**
 * **************************
 * MAIN QAFOOPROFILER CALLBACKS
 * **************************
 */

/**
 * This function gets called once when Qafoo Profiler gets enabled.
 * It replaces all the functions like zend_execute, zend_execute_internal,
 * etc that needs to be instrumented with their corresponding proxies.
 */
static void hp_begin(long level, long qafooprofiler_flags TSRMLS_DC)
{
	if (!hp_globals.enabled) {
		int hp_profile_flag = 1;

		hp_globals.enabled      = 1;
		hp_globals.qafooprofiler_flags = (uint32)qafooprofiler_flags;

		/* Replace zend_compile with our proxy */
		_zend_compile_file = zend_compile_file;
		zend_compile_file  = hp_compile_file;

		/* Replace zend_compile_string with our proxy */
		_zend_compile_string = zend_compile_string;
		zend_compile_string = hp_compile_string;

		/* Replace zend_execute with our proxy */
#if PHP_VERSION_ID < 50500
		_zend_execute = zend_execute;
#else
		_zend_execute_ex = zend_execute_ex;
#endif

		if (!(hp_globals.qafooprofiler_flags & QAFOOPROFILER_FLAGS_NO_USERLAND)) {
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

		qafooprofiler_original_error_cb = zend_error_cb;
		zend_error_cb = qafooprofiler_error_cb;

		zend_throw_exception_hook = qafooprofiler_throw_exception_hook;

		/* Replace zend_execute_internal with our proxy */
		_zend_execute_internal = zend_execute_internal;
		if (!(hp_globals.qafooprofiler_flags & QAFOOPROFILER_FLAGS_NO_BUILTINS)) {
			/* if NO_BUILTINS is not set (i.e. user wants to profile builtins),
			 * then we intercept internal (builtin) function calls.
			 */
			zend_execute_internal = hp_execute_internal;
		}

		/* Initialize with the dummy mode first Having these dummy callbacks saves
		 * us from checking if any of the callbacks are NULL everywhere. */
		hp_globals.mode_cb.init_cb     = hp_mode_dummy_init_cb;
		hp_globals.mode_cb.exit_cb     = hp_mode_dummy_exit_cb;
		hp_globals.mode_cb.begin_fn_cb = hp_mode_dummy_beginfn_cb;
		hp_globals.mode_cb.end_fn_cb   = hp_mode_dummy_endfn_cb;

		/* Register the appropriate callback functions Override just a subset of
		 * all the callbacks is OK. */
		switch(level) {
			case QAFOOPROFILER_MODE_HIERARCHICAL:
				hp_globals.mode_cb.begin_fn_cb = hp_mode_hier_beginfn_cb;
				hp_globals.mode_cb.end_fn_cb   = hp_mode_hier_endfn_cb;
				break;

			case QAFOOPROFILER_MODE_LAYER:
				hp_globals.mode_cb.begin_fn_cb = hp_mode_hier_beginfn_cb;
				hp_globals.mode_cb.end_fn_cb   = hp_mode_layer_endfn_cb;
				break;

			case QAFOOPROFILER_MODE_SAMPLED:
				hp_globals.mode_cb.init_cb     = hp_mode_sampled_init_cb;
				hp_globals.mode_cb.begin_fn_cb = hp_mode_sampled_beginfn_cb;
				hp_globals.mode_cb.end_fn_cb   = hp_mode_sampled_endfn_cb;
				break;
		}

		/* one time initializations */
		hp_init_profiler_state(level TSRMLS_CC);

		/* start profiling from fictitious main() */
		BEGIN_PROFILING(&hp_globals.entries, ROOT_SYMBOL, hp_profile_flag);
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
 * Called from qafooprofiler_disable(). Removes all the proxies setup by
 * hp_begin() and restores the original values.
 */
static void hp_stop(TSRMLS_D)
{
	int hp_profile_flag = 1;

	/* End any unfinished calls */
	while (hp_globals.entries) {
		END_PROFILING(&hp_globals.entries, hp_profile_flag);
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

	zend_error_cb = qafooprofiler_original_error_cb;
	zend_throw_exception_hook = NULL;

	/* Resore cpu affinity. */
	restore_cpu_affinity(&hp_globals.prev_mask);

	/* Stop profiling */
	hp_globals.enabled = 0;
}


/**
 * *****************************
 * QAFOOPROFILER ZVAL UTILITY FUNCTIONS
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
		for(; name_array[i] != NULL && i < QAFOOPROFILER_MAX_FILTERED_FUNCTIONS; i++) {
			efree(name_array[i]);
		}
		efree(name_array);
	}
}

static inline int hp_argument_entry(uint8 hash_code, char *curr_func)
{
	/* First check if argument functions is enabled */
	return hp_globals.argument_functions != NULL &&
		hp_function_map_exists(hp_globals.argument_functions, hash_code, curr_func);
}

/* {{{ gettraceasstring() macros */
#define QAFOOPROFILER_TRACE_APPEND_CHR(chr)				\
	*str = (char*)erealloc(*str, *len + 1 + 1);		\
	(*str)[(*len)++] = chr

#define QAFOOPROFILER_TRACE_APPEND_STRL(val, vallen)							\
	{                                                                   \
		int l = vallen;                                                 \
		*str = (char*)erealloc(*str, *len + l + 1);                     \
		memcpy((*str) + *len, val, l);                                  \
		*len += l;                                                      \
	}

#define QAFOOPROFILER_TRACE_APPEND_STR(val)                                            \
    QAFOOPROFILER_TRACE_APPEND_STRL(val, sizeof(val)-1)

#define QAFOOPROFILER_TRACE_APPEND_KEY(key)										\
	if (zend_hash_find(ht, key, sizeof(key), (void**)&tmp) == SUCCESS) {    \
		if (Z_TYPE_PP(tmp) != IS_STRING) {									\
			zend_error(E_WARNING, "Value for %s is no string", key);		\
			QAFOOPROFILER_TRACE_APPEND_STR("[unknown]");							\
		} else {															\
			QAFOOPROFILER_TRACE_APPEND_STRL(Z_STRVAL_PP(tmp), Z_STRLEN_PP(tmp));	\
		}																	\
	}


#define TRACE_ARG_APPEND(vallen)														\
	*str = (char*)erealloc(*str, *len + 1 + vallen);									\
	memmove((*str) + *len - l_added + 1 + vallen, (*str) + *len - l_added + 1, l_added);

/* }}} */

static int qafooprofiler_build_trace_string(zval **frame TSRMLS_DC, int num_args, va_list args, zend_hash_key *hash_key) /* {{{ */
{
	char *s_tmp, **str;
	int *len, *num;
	long line;
	HashTable *ht = Z_ARRVAL_PP(frame);
	zval **file, **tmp;

	if (Z_TYPE_PP(frame) != IS_ARRAY) {
		zend_error(E_WARNING, "Expected array for frame %lu", hash_key->h);
		return ZEND_HASH_APPLY_KEEP;
	}

	str = va_arg(args, char**);
	len = va_arg(args, int*);
	num = va_arg(args, int*);

	s_tmp = emalloc(1 + MAX_LENGTH_OF_LONG + 1 + 1);
	sprintf(s_tmp, "#%d ", (*num)++);
	QAFOOPROFILER_TRACE_APPEND_STRL(s_tmp, strlen(s_tmp));
	efree(s_tmp);
	if (zend_hash_find(ht, "file", sizeof("file"), (void**)&file) == SUCCESS) {
		if (Z_TYPE_PP(file) != IS_STRING) {
			zend_error(E_WARNING, "Function name is no string");
			QAFOOPROFILER_TRACE_APPEND_STR("[unknown function]");
		} else{
			if (zend_hash_find(ht, "line", sizeof("line"), (void**)&tmp) == SUCCESS) {
				if (Z_TYPE_PP(tmp) == IS_LONG) {
					line = Z_LVAL_PP(tmp);
				} else {
					zend_error(E_WARNING, "Line is no long");
					line = 0;
				}
			} else {
				line = 0;
			}
			s_tmp = emalloc(Z_STRLEN_PP(file) + MAX_LENGTH_OF_LONG + 4 + 1);
			sprintf(s_tmp, "%s(%ld): ", Z_STRVAL_PP(file), line);
			QAFOOPROFILER_TRACE_APPEND_STRL(s_tmp, strlen(s_tmp));
			efree(s_tmp);
		}
	} else {
		QAFOOPROFILER_TRACE_APPEND_STR("[internal function]: ");
	}
	QAFOOPROFILER_TRACE_APPEND_KEY("class");
	QAFOOPROFILER_TRACE_APPEND_KEY("type");
	QAFOOPROFILER_TRACE_APPEND_KEY("function");
	QAFOOPROFILER_TRACE_APPEND_STR("()\n");
	return ZEND_HASH_APPLY_KEEP;
}
/* }}} */

static hp_string *qafooprofiler_backtrace(TSRMLS_D)
{
	zval *trace;
	char *res, **str, *s_tmp;
	int res_len = 0, *len = &res_len, num = 0;

	res = estrdup("");
	str = &res;

	ALLOC_ZVAL(trace);
	Z_UNSET_ISREF_P(trace);
	Z_SET_REFCOUNT_P(trace, 0);

	zend_fetch_debug_backtrace(trace, 1, 0, 0 TSRMLS_CC);
	zend_hash_apply_with_arguments(Z_ARRVAL_P(trace) TSRMLS_CC, (apply_func_args_t)qafooprofiler_build_trace_string, 3, str, len, &num);

	s_tmp = emalloc(1 + MAX_LENGTH_OF_LONG + 7 + 1);
	sprintf(s_tmp, "#%d {main}", num);
	QAFOOPROFILER_TRACE_APPEND_STRL(s_tmp, strlen(s_tmp));
	efree(s_tmp);

	res[res_len] = '\0';

	return hp_create_string(res, res_len);
}

void qafooprofiler_store_error(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args TSRMLS_DC)
{
	va_list new_args;
	char *buffer;
	int buffer_len;

	/* We have to copy the arglist otherwise it will segfault in original error cb */
	va_copy(new_args, args);

	buffer_len = vspprintf(&buffer, PG(log_errors_max_len), format, new_args);

	if (!hp_globals.last_error) {
		hp_error_clean(hp_globals.last_error);
	}
	hp_globals.last_error = hp_error_create();

	hp_globals.last_error->line = (int)error_lineno;
	hp_globals.last_error->type = type;

	if (error_filename != NULL) {
		hp_globals.last_error->file = hp_create_string(error_filename, strlen(error_filename));
	}

	/* We need to see if we have an uncaught exception fatal error now */
	if (type == E_ERROR && strncmp(buffer, "Uncaught exception", 18) == 0 && hp_globals.last_exception) {
		hp_globals.last_error->code = hp_globals.last_exception->code;

		if (hp_globals.last_exception->message) {
			hp_globals.last_error->message = emalloc(sizeof(hp_string));
			memcpy(hp_globals.last_error->message, hp_globals.last_exception->message, sizeof(hp_string));
			hp_globals.last_error->message->value = estrdup(hp_globals.last_exception->message->value);
		}

		if (hp_globals.last_exception->class) {
			hp_globals.last_error->class = emalloc(sizeof(hp_string));
			memcpy(hp_globals.last_error->class, hp_globals.last_exception->class, sizeof(hp_string));
			hp_globals.last_error->class->value = estrdup(hp_globals.last_exception->class->value);
		}

		if (hp_globals.last_exception->trace) {
			hp_globals.last_error->trace = emalloc(sizeof(hp_string));
			memcpy(hp_globals.last_error->trace, hp_globals.last_exception->trace, sizeof(hp_string));
			hp_globals.last_error->trace->value = estrdup(hp_globals.last_exception->trace->value);
		}
	} else {
		hp_globals.last_error->message = hp_create_string(buffer, buffer_len);
#if PHP_VERSION_ID >= 50400
		hp_globals.last_error->trace = qafooprofiler_backtrace(TSRMLS_C);
#endif
	}

	efree(buffer);
}

void qafooprofiler_error_cb(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args)
{
	error_handling_t  error_handling;

	TSRMLS_FETCH();

#if (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION >= 3) || PHP_MAJOR_VERSION >= 6
	error_handling  = EG(error_handling);
#else
	error_handling  = PG(error_handling);
#endif

	if (error_handling == EH_NORMAL) {
		switch (type) {
			case E_ERROR:
			case E_CORE_ERROR:
			case E_USER_ERROR:
				qafooprofiler_store_error(type, error_filename, error_lineno, format, args TSRMLS_CC);
		}
	}

	qafooprofiler_original_error_cb(type, error_filename, error_lineno, format, args);
}

static void qafooprofiler_throw_exception_hook(zval *exception TSRMLS_DC)
{
	zend_class_entry *default_ce, *exception_ce;
	zval *tmp;
	hp_error *error;

	if (!exception) {
		return;
	}

	default_ce = zend_exception_get_default(TSRMLS_C);
	exception_ce = zend_get_class_entry(exception TSRMLS_CC);

	if (hp_globals.last_exception != NULL) {
		hp_error_clean(hp_globals.last_exception);
	}

	hp_globals.last_exception = hp_error_create();

	tmp = zend_read_property(default_ce, exception, "message", sizeof("message")-1, 0 TSRMLS_CC);
	hp_globals.last_exception->message = hp_zval_to_string(tmp);

	tmp = zend_read_property(default_ce, exception, "file", sizeof("file")-1, 0 TSRMLS_CC);
	hp_globals.last_exception->file = hp_zval_to_string(tmp);

	tmp = zend_read_property(default_ce, exception, "line", sizeof("line")-1, 0 TSRMLS_CC);
	hp_globals.last_exception->line = hp_zval_to_long(tmp);

	tmp = zend_read_property(default_ce, exception, "code", sizeof("code")-1, 0 TSRMLS_CC);
	hp_globals.last_exception->code = hp_zval_to_long(tmp);

	tmp = zend_read_property(default_ce, exception, "trace", sizeof("trace")-1, 0 TSRMLS_CC);
	hp_globals.last_exception->trace = hp_zval_to_string(tmp);

	hp_globals.last_exception->class = hp_create_string(exception_ce->name, exception_ce->name_length);
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

static hp_error *hp_error_create()
{
	hp_error *error;

	error = emalloc(sizeof(hp_error));
	error->type = 0;
	error->line = 0;
	error->code = -1;
	error->message = NULL;
	error->trace = NULL;
	error->file = NULL;
	error->class = NULL;

	return error;
}

static void hp_error_clean(hp_error *error)
{
	if (error == NULL) {
		return;
	}

	if (error->message) {
		hp_string_clean(error->message);
		efree(error->message);
	}

	if (error->class) {
		hp_string_clean(error->class);
		efree(error->class);
	}

	if (error->trace) {
		hp_string_clean(error->trace);
		efree(error->trace);
	}

	if (error->file) {
		hp_string_clean(error->file);
		efree(error->file);
	}

	efree(error);
}

static inline void hp_string_clean(hp_string *str)
{
	if (str == NULL) {
		return;
	}

	efree(str->value);
}

static void hp_error_to_zval(hp_error *error, zval *z)
{
	zval *line, *type, *code;

	if (error == NULL) {
		return;
	}

	MAKE_STD_ZVAL(line);
	ZVAL_LONG(line, error->line);

	MAKE_STD_ZVAL(type);
	ZVAL_LONG(type, error->type);

	MAKE_STD_ZVAL(code);
	ZVAL_LONG(code, error->code);

	array_init(z);
	add_assoc_zval(z, "message", hp_string_to_zval(error->message));
	add_assoc_zval(z, "trace", hp_string_to_zval(error->trace));
	add_assoc_zval(z, "file", hp_string_to_zval(error->file));
	add_assoc_zval(z, "type", type);
	add_assoc_zval(z, "line", line);

	if (error->class) {
		add_assoc_zval(z, "class", hp_string_to_zval(error->class));
		add_assoc_zval(z, "code", code);
	}
}
