#include "php.h"
#include "../php_tideways.h"
#include "../spans.h"

extern ZEND_DECLARE_MODULE_GLOBALS(hp)

long tw_span_create(char *category, size_t category_len TSRMLS_DC)
{
    zval *span, *starts, *stops;
    int idx;
    long parent = 0;

    if (TWG(spans) == NULL) {
        return -1;
    }

    idx = zend_hash_num_elements(Z_ARRVAL_P(TWG(spans)));

    // Hardcode a limit of 1500 spans for now, Daemon will re-filter again to 1000.
    // We assume web-requests and non-spammy worker/crons here, need a way to support
    // very long running scripts at some point.
    if (idx >= TWG(max_spans)) {
        long *idx_ptr = NULL;

        if (zend_hash_find(TWG(span_cache), category, category_len+1, (void **)&idx_ptr) == SUCCESS) {
            idx = *idx_ptr;

            if (idx > 1) {
                tw_span_annotate_long(idx, "trunc", 1 TSRMLS_CC);

                return idx;
            }
        }
    }

    MAKE_STD_ZVAL(span);
    MAKE_STD_ZVAL(starts);
    MAKE_STD_ZVAL(stops);

    array_init(span);
    array_init(starts);
    array_init(stops);

    add_assoc_stringl(span, "n", category, category_len, 1);
    add_assoc_zval(span, "b", starts);
    add_assoc_zval(span, "e", stops);

    if (parent > 0) {
        add_assoc_long(span, "p", parent);
    }

    zend_hash_index_update(Z_ARRVAL_P(TWG(spans)), idx, &span, sizeof(zval*), NULL);

    if (idx >= TWG(max_spans)) {
        zend_hash_update(TWG(span_cache), category, category_len+1, &idx, sizeof(long), NULL);
    }

    return idx;
}

static int tw_convert_to_string(void *pDest TSRMLS_DC)
{
    zval **zv = (zval **) pDest;

    convert_to_string_ex(zv);

    return ZEND_HASH_APPLY_KEEP;
}

void tw_span_annotate(long spanId, zval *annotations TSRMLS_DC)
{
    zval **span, **span_annotations, *span_annotations_ptr;
    HashTable *ht;
    char  *key;
    uint   key_len;
    zval **value_ptr_ptr, *value_ptr, tmp;
    ulong  idx;

    if (spanId == -1) {
        return;
    }

    if (zend_hash_index_find(Z_ARRVAL_P(TWG(spans)), spanId, (void **) &span) == FAILURE) {
        return;
    }

    if (zend_hash_find(Z_ARRVAL_PP(span), "a", sizeof("a"), (void **) &span_annotations) == FAILURE) {
        MAKE_STD_ZVAL(span_annotations_ptr);
        array_init(span_annotations_ptr);
        span_annotations = &span_annotations_ptr;
        add_assoc_zval(*span, "a", span_annotations_ptr);
    }
    
    ht = Z_ARRVAL_P(annotations);
    for (zend_hash_internal_pointer_reset(ht);
            zend_hash_has_more_elements(ht) == SUCCESS;
            zend_hash_move_forward(ht)) {

        zend_hash_get_current_key_ex(ht, &key, &key_len, &idx, 0, NULL);

        if (zend_hash_get_current_data(ht, (void**)&value_ptr_ptr) != SUCCESS) {
            continue;
        }
        
        value_ptr = *value_ptr_ptr;

        ZVAL_COPY_VALUE(&tmp, value_ptr);
        zval_copy_ctor(&tmp);
        convert_to_string(&tmp);

        add_assoc_stringl_ex(*span_annotations, key, strlen(key)+1, Z_STRVAL_P(&tmp), Z_STRLEN_P(&tmp), 1);

        zval_dtor(&tmp);
    }
}

void tw_span_annotate_long(long spanId, char *key, long value TSRMLS_DC)
{
    zval **span, **span_annotations, *annotation_value, *span_annotations_ptr;

    if (zend_hash_index_find(Z_ARRVAL_P(TWG(spans)), spanId, (void **) &span) == FAILURE) {
        return;
    }

    if (spanId == -1) {
        return;
    }

    if (zend_hash_find(Z_ARRVAL_PP(span), "a", sizeof("a"), (void **) &span_annotations) == FAILURE) {
        MAKE_STD_ZVAL(span_annotations_ptr);
        array_init(span_annotations_ptr);
        span_annotations = &span_annotations_ptr;
        add_assoc_zval(*span, "a", span_annotations_ptr);
    }

    MAKE_STD_ZVAL(annotation_value);
    ZVAL_LONG(annotation_value, value);
    convert_to_string_ex(&annotation_value);

    add_assoc_zval_ex(*span_annotations, key, strlen(key)+1, annotation_value);
}

void tw_span_annotate_string(long spanId, char *key, char *value, int copy TSRMLS_DC)
{
    zval **span, **span_annotations, *span_annotations_ptr;
    int len;

    if (spanId == -1) {
        return;
    }

    if (zend_hash_index_find(Z_ARRVAL_P(TWG(spans)), spanId, (void **) &span) == FAILURE) {
        return;
    }

    if (zend_hash_find(Z_ARRVAL_PP(span), "a", sizeof("a"), (void **) &span_annotations) == FAILURE) {
        MAKE_STD_ZVAL(span_annotations_ptr);
        array_init(span_annotations_ptr);
        span_annotations = &span_annotations_ptr;
        add_assoc_zval(*span, "a", span_annotations_ptr);
    }

    // limit size of annotations to 1000 characters, this mostly affects "sql"
    // annotations, but the daemon sql parser is resilent against broken SQL.
    len = strlen(value);
    if (copy == 1 && len > TIDEWAYS_ANNOTATION_MAX_LENGTH) {
        len = TIDEWAYS_ANNOTATION_MAX_LENGTH;
    }

    add_assoc_stringl_ex(*span_annotations, key, strlen(key)+1, value, len, copy);
}
