#include "timer.h"

#define TIDEWAYS_XHPROF_ROOT_SYMBOL "main()"
#define TIDEWAYS_XHPROF_CALLGRAPH_COUNTER_SIZE 1024
#define TIDEWAYS_XHPROF_CALLGRAPH_SLOTS 8192
#define TIDEWAYS_XHPROF_FLAGS_CPU 1
#define TIDEWAYS_XHPROF_FLAGS_MEMORY_MU 2
#define TIDEWAYS_XHPROF_FLAGS_MEMORY_PMU 4
#define TIDEWAYS_XHPROF_FLAGS_MEMORY 6
#define TIDEWAYS_XHPROF_FLAGS_MEMORY_ALLOC 16
#define TIDEWAYS_XHPROF_FLAGS_MEMORY_ALLOC_AS_MU (32|16)
#define TIDEWAYS_XHPROF_FLAGS_NO_BUILTINS 8

void tracing_callgraph_append_to_array(zval *return_value TSRMLS_DC);
void tracing_callgraph_get_parent_child_name(xhprof_callgraph_bucket *bucket, char *symbol, size_t symbol_len TSRMLS_DC);
zend_ulong tracing_callgraph_bucket_key(xhprof_frame_t *frame);
xhprof_callgraph_bucket *tracing_callgraph_bucket_find(xhprof_callgraph_bucket *bucket, xhprof_frame_t *current_frame, xhprof_frame_t *previous, zend_long key);
void tracing_callgraph_bucket_free(xhprof_callgraph_bucket *bucket);
void tracing_begin(zend_long flags TSRMLS_DC);
void tracing_end(TSRMLS_D);
void tracing_enter_root_frame(TSRMLS_D);
void tracing_request_init(TSRMLS_D);
void tracing_request_shutdown();
void tracing_determine_clock_source();

#define TXRG(v) ZEND_MODULE_GLOBALS_ACCESSOR(tideways_xhprof, v)

#if defined(ZTS) && defined(COMPILE_DL_TIDEWAYS_XHPROF)
ZEND_TSRMLS_CACHE_EXTERN()
#endif

static zend_always_inline void tracing_fast_free_frame(xhprof_frame_t *p TSRMLS_DC)
{
    if (p->function_name != NULL) {
        zend_string_release(p->function_name);
    }
    if (p->class_name != NULL) {
        zend_string_release(p->class_name);
    }

    /* we use/overload the previous_frame field in the structure to link entries in
     * the free list. */
    p->previous_frame = TXRG(frame_free_list);
    TXRG(frame_free_list) = p;
}

static zend_always_inline xhprof_frame_t* tracing_fast_alloc_frame(TSRMLS_D)
{
    xhprof_frame_t *p;

    p = TXRG(frame_free_list);

    if (p) {
        TXRG(frame_free_list) = p->previous_frame;
        return p;
    } else {
        return (xhprof_frame_t *)emalloc(sizeof(xhprof_frame_t));
    }
}

static zend_always_inline zend_string* tracing_get_class_name(zend_execute_data *data TSRMLS_DC)
{
    zend_function *curr_func;

    if (!data) {
        return NULL;
    }

    curr_func = data->func;

    if (curr_func->common.scope != NULL) {
        zend_string_addref(curr_func->common.scope->name);

        return curr_func->common.scope->name;
    }

    return NULL;
}

static zend_always_inline zend_string* tracing_get_function_name(zend_execute_data *data TSRMLS_DC)
{
    zend_function *curr_func;

    if (!data) {
        return NULL;
    }

    curr_func = data->func;

    if (!curr_func->common.function_name) {
        // This branch includes execution of eval and include/require(_once) calls
        // We assume it is not 1999 anymore and not much PHP code runs in the
        // body of a file and if it is, we are ok with adding it to the caller's wt.
        return NULL;
    }

    zend_string_addref(curr_func->common.function_name);

    return curr_func->common.function_name;
}

zend_always_inline static int tracing_enter_frame_callgraph(zend_string *root_symbol, zend_execute_data *execute_data TSRMLS_DC)
{
    zend_string *function_name = (root_symbol != NULL) ? zend_string_copy(root_symbol) : tracing_get_function_name(execute_data TSRMLS_CC);
    xhprof_frame_t *current_frame;
    xhprof_frame_t *p;
    int recurse_level = 0;

    if (function_name == NULL) {
        return 0;
    }

    current_frame = tracing_fast_alloc_frame(TSRMLS_C);
    current_frame->class_name = (root_symbol == NULL) ? tracing_get_class_name(execute_data TSRMLS_CC) : NULL;
    current_frame->function_name = function_name;
    current_frame->previous_frame = TXRG(callgraph_frames);
    current_frame->recurse_level = 0;
    current_frame->wt_start = time_milliseconds(TXRG(clock_source), TXRG(timebase_factor));

    if (TXRG(flags) & TIDEWAYS_XHPROF_FLAGS_CPU) {
        current_frame->cpu_start = cpu_timer();
    }

    if (TXRG(flags) & TIDEWAYS_XHPROF_FLAGS_MEMORY_PMU) {
        current_frame->pmu_start = zend_memory_peak_usage(0 TSRMLS_CC);
    }

    if (TXRG(flags) & TIDEWAYS_XHPROF_FLAGS_MEMORY_MU) {
        current_frame->mu_start = zend_memory_usage(0 TSRMLS_CC);
    }

    current_frame->num_alloc = TXRG(num_alloc);
    current_frame->num_free = TXRG(num_free);
    current_frame->amount_alloc = TXRG(amount_alloc);

    /* We only need to compute the hash for the function name,
     * that should be "good" enough, we sort into 1024 buckets only anyways */
    current_frame->hash_code = ZSTR_HASH(function_name) % TIDEWAYS_XHPROF_CALLGRAPH_COUNTER_SIZE;

    /* Update entries linked list */
    TXRG(callgraph_frames) = current_frame;

    if (TXRG(function_hash_counters)[current_frame->hash_code] > 0) {
        /* Find this symbols recurse level */
        for(p = current_frame->previous_frame; p; p = p->previous_frame) {
            if (current_frame->function_name == p->function_name && (!current_frame->class_name || current_frame->class_name == p->class_name)) {
                recurse_level = (p->recurse_level) + 1;
                break;
            }
        }
    }
    TXRG(function_hash_counters)[current_frame->hash_code]++;

    /* Init current function's recurse level */
    current_frame->recurse_level = recurse_level;

    return 1;
}

zend_always_inline static void tracing_exit_frame_callgraph(TSRMLS_D)
{
    xhprof_frame_t *current_frame = TXRG(callgraph_frames);
    xhprof_frame_t *previous = current_frame->previous_frame;
    zend_long duration = time_milliseconds(TXRG(clock_source), TXRG(timebase_factor)) - current_frame->wt_start;

    zend_ulong key = tracing_callgraph_bucket_key(current_frame);
    unsigned int slot = (unsigned int)key % TIDEWAYS_XHPROF_CALLGRAPH_SLOTS;
    xhprof_callgraph_bucket *bucket = TXRG(callgraph_buckets)[slot];

    bucket = tracing_callgraph_bucket_find(bucket, current_frame, previous, key);

    if (bucket == NULL) {
        bucket = emalloc(sizeof(xhprof_callgraph_bucket));
        bucket->key = key;
        bucket->child_class = current_frame->class_name ? zend_string_copy(current_frame->class_name) : NULL;
        bucket->child_function = zend_string_copy(current_frame->function_name);

        if (previous) {
            bucket->parent_class = previous->class_name ? zend_string_copy(current_frame->previous_frame->class_name) : NULL;
            bucket->parent_function = zend_string_copy(previous->function_name);
            bucket->parent_recurse_level = previous->recurse_level;
        } else {
            bucket->parent_class = NULL;
            bucket->parent_function = NULL;
            bucket->parent_recurse_level = 0;
        }

        bucket->count = 0;
        bucket->wall_time = 0;
        bucket->cpu_time = 0;
        bucket->memory = 0;
        bucket->memory_peak = 0;
        bucket->num_alloc = 0;
        bucket->num_free = 0;
        bucket->amount_alloc = 0;
        bucket->child_recurse_level = current_frame->recurse_level;
        bucket->next = TXRG(callgraph_buckets)[slot];

        TXRG(callgraph_buckets)[slot] = bucket;
    }

    bucket->count++;
    bucket->wall_time += duration;

    bucket->num_alloc += TXRG(num_alloc) - current_frame->num_alloc;
    bucket->num_free += TXRG(num_free) - current_frame->num_free;
    bucket->amount_alloc += TXRG(amount_alloc) - current_frame->amount_alloc;

    if (TXRG(flags) & TIDEWAYS_XHPROF_FLAGS_CPU) {
        bucket->cpu_time += (cpu_timer() - current_frame->cpu_start);
    }

    if (TXRG(flags) & TIDEWAYS_XHPROF_FLAGS_MEMORY_MU) {
        bucket->memory += (zend_memory_usage(0 TSRMLS_CC) - current_frame->mu_start);
    }

    if (TXRG(flags) & TIDEWAYS_XHPROF_FLAGS_MEMORY_PMU) {
        bucket->memory_peak += (zend_memory_peak_usage(0 TSRMLS_CC) - current_frame->pmu_start);
    }

    TXRG(function_hash_counters)[current_frame->hash_code]--;

    TXRG(callgraph_frames) = TXRG(callgraph_frames)->previous_frame;
    tracing_fast_free_frame(current_frame TSRMLS_CC);
}
