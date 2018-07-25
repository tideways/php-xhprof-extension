#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "ext/standard/html.h"
#include "php_tideways_xhprof.h"

extern ZEND_DECLARE_MODULE_GLOBALS(tideways_xhprof);

#include "tracing.h"

static const char digits[] = "0123456789abcdef";

static void *(*_zend_malloc) (size_t);
static void (*_zend_free) (void *);
static void *(*_zend_realloc) (void *, size_t);

void *tideways_malloc (size_t size);
void tideways_free (void *ptr);
void *tideways_realloc (void *ptr, size_t size);

void tracing_determine_clock_source(TSRMLS_D) {
#ifdef __APPLE__
    TXRG(clock_source) = TIDEWAYS_XHPROF_CLOCK_MACH;
#elif defined(__powerpc__) || defined(__ppc__)
    TXRG(clock_source) = TIDEWAYS_XHPROF_CLOCK_TSC;
#elif defined(PHP_WIN32)
    TXRG(clock_source) = TIDEWAYS_XHPROF_CLOCK_QPC;
#else
    struct timespec res;

    if (TXRG(clock_use_rdtsc) == 1) {
        TXRG(clock_source) = TIDEWAYS_XHPROF_CLOCK_TSC;
    } else if (clock_gettime(CLOCK_MONOTONIC, &res) == 0) {
        TXRG(clock_source) = TIDEWAYS_XHPROF_CLOCK_CGT;
    } else {
        TXRG(clock_source) = TIDEWAYS_XHPROF_CLOCK_GTOD;
    }
#endif
}

/**
 * Free any items in the free list.
 */
static zend_always_inline void tracing_free_the_free_list(TSRMLS_D)
{
    xhprof_frame_t *frame = TXRG(frame_free_list);
    xhprof_frame_t *current;

    while (frame) {
        current = frame;
        frame = frame->previous_frame;
        efree(current);
    }
}

void tracing_enter_root_frame(TSRMLS_D)
{
    TXRG(start_time) = time_milliseconds(TXRG(clock_source), TXRG(timebase_factor));
    TXRG(start_timestamp) = current_timestamp();
    TXRG(enabled) = 1;
    TXRG(root) = zend_string_init(TIDEWAYS_XHPROF_ROOT_SYMBOL, sizeof(TIDEWAYS_XHPROF_ROOT_SYMBOL)-1, 0);

    tracing_enter_frame_callgraph(TXRG(root), NULL TSRMLS_CC);
}

void tracing_end(TSRMLS_D)
{
    if (TXRG(enabled) == 1) {
        if (TXRG(root)) {
            zend_string_release(TXRG(root));
        }

        while (TXRG(callgraph_frames)) {
            tracing_exit_frame_callgraph(TSRMLS_C);
        }

        TXRG(enabled) = 0;
        TXRG(callgraph_frames) = NULL;

        if (TXRG(flags) & TIDEWAYS_XHPROF_FLAGS_MEMORY_ALLOC) {
            zend_mm_heap *heap = zend_mm_get_heap();

            if (_zend_malloc || _zend_free || _zend_realloc) {
                zend_mm_set_custom_handlers(heap, _zend_malloc, _zend_free, _zend_realloc);
                _zend_malloc = NULL;
                _zend_free = NULL;
                _zend_realloc = NULL;
            } else {
                // zend_mm_heap is incomplete type, hence one can not access it
                //  the following line is equivalent to heap->use_custom_heap = 0;
                *((int*) heap) = 0;
            }
        }
    }
}

void tracing_callgraph_bucket_free(xhprof_callgraph_bucket *bucket)
{
    if (bucket->parent_class) {
        zend_string_release(bucket->parent_class);
    }

    if (bucket->parent_function) {
        zend_string_release(bucket->parent_function);
    }

    if (bucket->child_class) {
        zend_string_release(bucket->child_class);
    }

    if (bucket->child_function) {
        zend_string_release(bucket->child_function);
    }

    efree(bucket);
}

xhprof_callgraph_bucket *tracing_callgraph_bucket_find(xhprof_callgraph_bucket *bucket, xhprof_frame_t *current_frame, xhprof_frame_t *previous, zend_long key)
{
    while (bucket) {
        if (bucket->key == key &&
            bucket->child_recurse_level == current_frame->recurse_level &&
            bucket->child_class == current_frame->class_name &&
            bucket->child_function == current_frame->function_name) {

            if (previous == NULL && bucket->parent_class == NULL && bucket->parent_function == NULL ) {
                // special handling for the root
                return bucket;
            } else if (previous &&
                       previous->recurse_level == bucket->parent_recurse_level &&
                       previous->class_name == bucket->parent_class &&
                       previous->function_name == bucket->parent_function) {
                // parent matches as well
                return bucket;
            }
        }

        bucket = bucket->next;
    }

    return NULL;
}

zend_always_inline static zend_ulong hash_data(zend_ulong hash, char *data, size_t size)
{
    size_t i;

    for (i = 0; i < size; ++i) {
        hash = hash * 33 + data[i];
    }

    return hash;
}

zend_always_inline static zend_ulong hash_int(zend_ulong hash, int data)
{
    return hash_data(hash, (char*) &data, sizeof(data));
}

zend_ulong tracing_callgraph_bucket_key(xhprof_frame_t *frame)
{
    zend_ulong hash = 5381;
    xhprof_frame_t *previous = frame->previous_frame;

    if (previous) {
        if (previous->class_name) {
            hash = hash_int(hash, ZSTR_HASH(previous->class_name));
        }

        if (previous->function_name) {
            hash = hash_int(hash, ZSTR_HASH(previous->function_name));
        }
        hash += previous->recurse_level;
    }

    if (frame->class_name) {
        hash = hash_int(hash, ZSTR_HASH(frame->class_name));
    }

    if (frame->function_name) {
        hash = hash_int(hash, ZSTR_HASH(frame->function_name));
    }

    hash += frame->recurse_level;

    return hash;
}

void tracing_callgraph_get_parent_child_name(xhprof_callgraph_bucket *bucket, char *symbol, size_t symbol_len TSRMLS_DC)
{
    if (bucket->parent_class) {
        if (bucket->parent_recurse_level > 0) {
            snprintf(symbol, symbol_len, "%s::%s@%d==>", ZSTR_VAL(bucket->parent_class), ZSTR_VAL(bucket->parent_function), bucket->parent_recurse_level);
        } else {
            snprintf(symbol, symbol_len, "%s::%s==>", ZSTR_VAL(bucket->parent_class), ZSTR_VAL(bucket->parent_function));
        }
    } else if (bucket->parent_function) {
        if (bucket->parent_recurse_level > 0) {
            snprintf(symbol, symbol_len, "%s@%d==>", ZSTR_VAL(bucket->parent_function), bucket->parent_recurse_level);
        } else {
            snprintf(symbol, symbol_len, "%s==>", ZSTR_VAL(bucket->parent_function));
        }
    } else {
        snprintf(symbol, symbol_len, "");
    }

    if (bucket->child_class) {
        if (bucket->child_recurse_level > 0) {
            snprintf(symbol, symbol_len, "%s%s::%s@%d", symbol, ZSTR_VAL(bucket->child_class), ZSTR_VAL(bucket->child_function), bucket->child_recurse_level);
        } else {
            snprintf(symbol, symbol_len, "%s%s::%s", symbol, ZSTR_VAL(bucket->child_class), ZSTR_VAL(bucket->child_function));
        }
    } else if (bucket->child_function) {
        if (bucket->child_recurse_level > 0) {
            snprintf(symbol, symbol_len, "%s%s@%d", symbol, ZSTR_VAL(bucket->child_function), bucket->child_recurse_level);
        } else {
            snprintf(symbol, symbol_len, "%s%s", symbol, ZSTR_VAL(bucket->child_function));
        }
    }
}

void tracing_callgraph_append_to_array(zval *return_value TSRMLS_DC)
{
    int i = 0;
    xhprof_callgraph_bucket *bucket;
    char symbol[512] = "";
    zval stats_zv, *stats = &stats_zv;

    int as_mu =
        (TXRG(flags) & (TIDEWAYS_XHPROF_FLAGS_MEMORY_ALLOC_AS_MU | TIDEWAYS_XHPROF_FLAGS_MEMORY_MU))
            == TIDEWAYS_XHPROF_FLAGS_MEMORY_ALLOC_AS_MU;

    for (i = 0; i < TIDEWAYS_XHPROF_CALLGRAPH_SLOTS; i++) {
        bucket = TXRG(callgraph_buckets)[i];

        while (bucket) {
            tracing_callgraph_get_parent_child_name(bucket, symbol, sizeof(symbol) TSRMLS_CC);

            array_init(stats);
            add_assoc_long(stats, "ct", bucket->count);
            add_assoc_long(stats, "wt", bucket->wall_time);

            if (TXRG(flags) & TIDEWAYS_XHPROF_FLAGS_MEMORY_ALLOC) {
                add_assoc_long(stats, "mem.na", bucket->num_alloc);
                add_assoc_long(stats, "mem.nf", bucket->num_free);
                add_assoc_long(stats, "mem.aa", bucket->amount_alloc);

                if (as_mu) {
                    add_assoc_long(stats, "mu", bucket->amount_alloc);
                }
            }

            if (TXRG(flags) & TIDEWAYS_XHPROF_FLAGS_CPU) {
                add_assoc_long(stats, "cpu", bucket->cpu_time);
            }

            if (TXRG(flags) & TIDEWAYS_XHPROF_FLAGS_MEMORY_MU) {
                add_assoc_long(stats, "mu", bucket->memory);
            }

            if (TXRG(flags) & TIDEWAYS_XHPROF_FLAGS_MEMORY_PMU) {
                add_assoc_long(stats, "pmu", bucket->memory_peak);
            }

            add_assoc_zval(return_value, symbol, stats);

            TXRG(callgraph_buckets)[i] = bucket->next;
            tracing_callgraph_bucket_free(bucket);
            bucket = TXRG(callgraph_buckets)[i];
        }
    }
}

void tracing_begin(zend_long flags TSRMLS_DC)
{
    int i;

    TXRG(flags) = flags;
    TXRG(callgraph_frames) = NULL;

    for (i = 0; i < TIDEWAYS_XHPROF_CALLGRAPH_SLOTS; i++) {
        TXRG(callgraph_buckets)[i] = NULL;
    }

    for (i = 0; i < TIDEWAYS_XHPROF_CALLGRAPH_COUNTER_SIZE; i++) {
        TXRG(function_hash_counters)[i] = 0;
    }

    if (flags & TIDEWAYS_XHPROF_FLAGS_MEMORY_ALLOC) {
        zend_mm_heap *heap = zend_mm_get_heap();
        zend_mm_get_custom_handlers (heap, &_zend_malloc, &_zend_free, &_zend_realloc);
        zend_mm_set_custom_handlers (heap, &tideways_malloc, &tideways_free, &tideways_realloc);
    }
}

void tracing_request_init(TSRMLS_D)
{
    TXRG(timebase_factor) = get_timebase_factor(TXRG(clock_source));
    TXRG(enabled) = 0;
    TXRG(flags) = 0;
    TXRG(frame_free_list) = NULL;

    TXRG(num_alloc) = 0;
    TXRG(num_free) = 0;
    TXRG(amount_alloc) = 0;
}

void tracing_request_shutdown()
{
    tracing_free_the_free_list(TSRMLS_C);
}

void *tideways_malloc (size_t size)
{
    TXRG(num_alloc) += 1;
    TXRG(amount_alloc) += size;

    if (_zend_malloc) {
        return _zend_malloc(size);
    }

    zend_mm_heap *heap = zend_mm_get_heap();
    return zend_mm_alloc(heap, size);
}

void tideways_free (void *ptr)
{
    TXRG(num_free) += 1;

    if (_zend_free) {
        return _zend_free(ptr);
    }

    zend_mm_heap *heap = zend_mm_get_heap();
    return zend_mm_free(heap, ptr);
}

void *tideways_realloc (void *ptr, size_t size)
{
    TXRG(num_alloc) += 1;
    TXRG(num_free) += 1;
    TXRG(amount_alloc) += size;

    if (_zend_realloc) {
        return _zend_realloc(ptr, size);
    }

    zend_mm_heap *heap = zend_mm_get_heap();
    return zend_mm_realloc(heap, ptr, size);
}
