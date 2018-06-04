#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "SAPI.h"
#include "ext/standard/info.h"
#include "php_tideways_xhprof.h"

ZEND_DECLARE_MODULE_GLOBALS(tideways_xhprof)

#include "tracing.h"

static void (*_zend_execute_ex) (zend_execute_data *execute_data);
static void (*_zend_execute_internal) (zend_execute_data *execute_data, zval *return_value);
ZEND_DLEXPORT void tideways_xhprof_execute_internal(zend_execute_data *execute_data, zval *return_value);
ZEND_DLEXPORT void tideways_xhprof_execute_ex (zend_execute_data *execute_data);

static void *(*_zend_malloc) (size_t);
static void (*_zend_free) (void *);
static void *(*_zend_realloc) (void *, size_t);

void *tideways_malloc (size_t size);
void tideways_free (void *ptr);
void *tideways_realloc (void *ptr, size_t size);

PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("tideways_xhprof.clock_use_rdtsc", "0", PHP_INI_ALL, OnUpdateBool, clock_use_rdtsc, zend_tideways_xhprof_globals, tideways_xhprof_globals)
PHP_INI_END()

PHP_FUNCTION(tideways_xhprof_enable)
{
    zend_long flags = 0;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &flags) == FAILURE) {
        return;
    }

    tracing_begin(flags TSRMLS_CC);
    tracing_enter_root_frame(TSRMLS_C);
}

PHP_FUNCTION(tideways_xhprof_disable)
{
    tracing_end(TSRMLS_C);

    array_init(return_value);

    tracing_callgraph_append_to_array(return_value TSRMLS_CC);
}

PHP_GINIT_FUNCTION(tideways_xhprof)
{
#if defined(COMPILE_DL_TIDEWAYS_XHPROF) && defined(ZTS)
     ZEND_TSRMLS_CACHE_UPDATE();
#endif 
     tideways_xhprof_globals->root = NULL;
     tideways_xhprof_globals->callgraph_frames = NULL;
     tideways_xhprof_globals->frame_free_list = NULL;
}

PHP_MINIT_FUNCTION(tideways_xhprof)
{
    REGISTER_INI_ENTRIES();

    REGISTER_LONG_CONSTANT("TIDEWAYS_XHPROF_FLAGS_MEMORY", TIDEWAYS_XHPROF_FLAGS_MEMORY, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("TIDEWAYS_XHPROF_FLAGS_MEMORY_MU", TIDEWAYS_XHPROF_FLAGS_MEMORY_MU, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("TIDEWAYS_XHPROF_FLAGS_MEMORY_PMU", TIDEWAYS_XHPROF_FLAGS_MEMORY_PMU, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("TIDEWAYS_XHPROF_FLAGS_CPU", TIDEWAYS_XHPROF_FLAGS_CPU, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("TIDEWAYS_XHPROF_FLAGS_NO_BUILTINS", TIDEWAYS_XHPROF_FLAGS_NO_BUILTINS, CONST_CS | CONST_PERSISTENT);

    _zend_execute_internal = zend_execute_internal;
    zend_execute_internal = tideways_xhprof_execute_internal;

    _zend_execute_ex = zend_execute_ex;
    zend_execute_ex = tideways_xhprof_execute_ex;

    zend_mm_heap *heap = zend_mm_get_heap();
    zend_mm_get_custom_handlers (heap, &_zend_malloc, &_zend_free, &_zend_realloc);
    zend_mm_set_custom_handlers (heap, &tideways_malloc, &tideways_free, &tideways_realloc);

    

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(tideways_xhprof)
{
    zend_mm_heap *heap = zend_mm_get_heap();
    *((int *) heap) = 0;
    return SUCCESS;
}

PHP_RINIT_FUNCTION(tideways_xhprof)
{
    tracing_request_init(TSRMLS_C);
    tracing_determine_clock_source(TSRMLS_C);

    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(tideways_xhprof)
{
    int i = 0;
    xhprof_callgraph_bucket *bucket;

    tracing_end(TSRMLS_C);

    for (i = 0; i < TIDEWAYS_XHPROF_CALLGRAPH_SLOTS; i++) {
        bucket = TXRG(callgraph_buckets)[i];

        while (bucket) {
            TXRG(callgraph_buckets)[i] = bucket->next;
            tracing_callgraph_bucket_free(bucket);
            bucket = TXRG(callgraph_buckets)[i];
        }
    }

    tracing_request_shutdown();

    return SUCCESS;
}

static int tideways_xhprof_info_print(const char *str) /* {{{ */
{
    return php_output_write(str, strlen(str));
}

PHP_MINFO_FUNCTION(tideways_xhprof)
{
    php_info_print_table_start();
    php_info_print_table_row(2, "Version", PHP_TIDEWAYS_XHPROF_VERSION);

    switch (TXRG(clock_source)) {
        case TIDEWAYS_XHPROF_CLOCK_TSC:
            php_info_print_table_row(2, "Clock Source", "tsc");
            break;
        case TIDEWAYS_XHPROF_CLOCK_CGT:
            php_info_print_table_row(2, "Clock Source", "clock_gettime");
            break;
        case TIDEWAYS_XHPROF_CLOCK_GTOD:
            php_info_print_table_row(2, "Clock Source", "gettimeofday");
            break;
        case TIDEWAYS_XHPROF_CLOCK_MACH:
            php_info_print_table_row(2, "Clock Source", "mach");
            break;
        case TIDEWAYS_XHPROF_CLOCK_QPC:
            php_info_print_table_row(2, "Clock Source", "Query Performance Counter");
            break;
        case TIDEWAYS_XHPROF_CLOCK_NONE:
            php_info_print_table_row(2, "Clock Source", "none");
            break;
    }
    php_info_print_table_end();

    php_info_print_box_start(0);

    if (!sapi_module.phpinfo_as_text) {
        tideways_xhprof_info_print("<a href=\"https://tideways.io\"><img border=0 src=\"");
        tideways_xhprof_info_print(TIDEWAYS_LOGO_DATA_URI "\" alt=\"Tideways logo\" /></a>\n");
    }

    tideways_xhprof_info_print("Tideways is a PHP Profiler, Monitoring and Exception Tracking Software.");
    tideways_xhprof_info_print(!sapi_module.phpinfo_as_text?"<br /><br />":"\n\n");
    tideways_xhprof_info_print("The 'tideways_xhprof' extension provides a subset of the functionality of our commercial Tideways offering in a modern, optimized fork of the XHProf extension from Facebook as open-source. (c) Tideways GmbH 2014-2017, (c) Facebook 2009");

    if (!sapi_module.phpinfo_as_text) {
        tideways_xhprof_info_print("<br /><br /><strong>Register for a free trial on <a style=\"background-color: inherit\" href=\"https://tideways.io\">https://tideways.io</a></strong>");
    } else {
        tideways_xhprof_info_print("\n\nRegister for a free trial on https://tideways.io\n\n");
    }

    php_info_print_box_end();

}

ZEND_DLEXPORT void tideways_xhprof_execute_internal(zend_execute_data *execute_data, zval *return_value) {
    int is_profiling = 1;

    if (!TXRG(enabled) || (TXRG(flags) & TIDEWAYS_XHPROF_FLAGS_NO_BUILTINS) > 0) {
        execute_internal(execute_data, return_value TSRMLS_CC);
        return;
    }

    is_profiling = tracing_enter_frame_callgraph(NULL, execute_data TSRMLS_CC);

    if (!_zend_execute_internal) {
        execute_internal(execute_data, return_value TSRMLS_CC);
    } else {
        _zend_execute_internal(execute_data, return_value TSRMLS_CC);
    }

    if (is_profiling == 1 && TXRG(callgraph_frames)) {
        tracing_exit_frame_callgraph(TSRMLS_C);
    }
}

ZEND_DLEXPORT void tideways_xhprof_execute_ex (zend_execute_data *execute_data) {
    zend_execute_data *real_execute_data = execute_data;
    int is_profiling = 0;

    if (!TXRG(enabled)) {
        _zend_execute_ex(execute_data TSRMLS_CC);
        return;
    }

    is_profiling = tracing_enter_frame_callgraph(NULL, real_execute_data TSRMLS_CC);

    _zend_execute_ex(execute_data TSRMLS_CC);

    if (is_profiling == 1 && TXRG(callgraph_frames)) {
        tracing_exit_frame_callgraph(TSRMLS_C);
    }
}

void *tideways_malloc (size_t size)
{
    xhprof_frame_t *current_frame = TXRG(callgraph_frames);
    if (current_frame) {
        current_frame->num_alloc += 1;
        current_frame->amount_alloc += size;
    }

    if (_zend_malloc) {
        return _zend_malloc(size);
    }

    zend_mm_heap *heap = zend_mm_get_heap();
    return zend_mm_alloc(heap, size);
}

void tideways_free (void *ptr)
{
    xhprof_frame_t *current_frame = TXRG(callgraph_frames);
    if (current_frame) {
        current_frame->num_free += 1;
    }

    if (_zend_free) {
        return _zend_free(ptr);
    }

    zend_mm_heap *heap = zend_mm_get_heap();
    return zend_mm_free(heap, ptr);
}

void *tideways_realloc (void *ptr, size_t size)
{
    xhprof_frame_t *current_frame = TXRG(callgraph_frames);
    if (current_frame) {
        current_frame->num_alloc += 1;
        current_frame->num_free += 1;
        current_frame->amount_alloc += size;
    }
    
    if (_zend_realloc) {
        return _zend_realloc(ptr, size);
    }

    zend_mm_heap *heap = zend_mm_get_heap();
    return zend_mm_realloc(heap, ptr, size);
}

const zend_function_entry tideways_xhprof_functions[] = {
    PHP_FE(tideways_xhprof_enable,	NULL)
    PHP_FE(tideways_xhprof_disable,	NULL)
    PHP_FE_END
};

zend_module_entry tideways_xhprof_module_entry = {
    STANDARD_MODULE_HEADER,
    "tideways_xhprof",
    tideways_xhprof_functions,
    PHP_MINIT(tideways_xhprof),
    PHP_MSHUTDOWN(tideways_xhprof),
    PHP_RINIT(tideways_xhprof),
    PHP_RSHUTDOWN(tideways_xhprof),
    PHP_MINFO(tideways_xhprof),
    PHP_TIDEWAYS_XHPROF_VERSION,
    PHP_MODULE_GLOBALS(tideways_xhprof),
    PHP_GINIT(tideways_xhprof),
    NULL,
    NULL,
    STANDARD_MODULE_PROPERTIES_EX
};

#ifdef COMPILE_DL_TIDEWAYS_XHPROF
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(tideways_xhprof)
#endif
