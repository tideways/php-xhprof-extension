#include "php.h"
#include "../php_tideways.h"
#include "../spans.h"

extern ZEND_DECLARE_MODULE_GLOBALS(hp)

long tw_span_create(char *category, size_t category_len TSRMLS_DC)
{
    zval span, starts, stops;
    int idx;
    long parent = 0;

    if (Z_TYPE(TWG(spans)) != IS_ARRAY) {
        return -1;
    }

    idx = zend_hash_num_elements(Z_ARRVAL(TWG(spans)));

    // If the max spans limit is reached, then we aggregate results on a single
    // span per category and mark it as "truncated" such that user interfaces
    // can detect these kind of spans and give them a proper name.
    if (idx >= TWG(max_spans)) {
        zval *zv;

        if (zv = zend_hash_str_find(TWG(span_cache), category, category_len)) {
            idx = Z_LVAL_P(zv);

            if (idx > -1) {
                tw_span_annotate_long(idx, "trunc", 1 TSRMLS_CC);

                return idx;
            }
        }
    }

    array_init(&span);
    array_init(&starts);
    array_init(&stops);

    add_assoc_stringl(&span, "n", category, category_len);
    add_assoc_zval(&span, "b", &starts);
    add_assoc_zval(&span, "e", &stops);

    if (parent > 0) {
        add_assoc_long(&span, "p", parent);
    }

    zend_hash_index_update(Z_ARRVAL(TWG(spans)), idx, &span);

    if (idx >= TWG(max_spans)) {
        zval zv;

        ZVAL_LONG(&zv, idx);
        zend_hash_str_update(TWG(span_cache), category, category_len, &zv);
    }

    return idx;
}

static int tw_convert_to_string(zval *zv)
{
    convert_to_string_ex(zv);

    return ZEND_HASH_APPLY_KEEP;
}

void tw_span_annotate(long spanId, zval *annotations TSRMLS_DC)
{
    zval *span, *span_annotations, span_annotations_value, *zv;
    zend_string *key, *annotation_value;
    ulong num_key;

    if (spanId == -1) {
        return;
    }

    span = zend_hash_index_find(Z_ARRVAL(TWG(spans)), spanId);

    if (span == NULL) {
        return;
    }

    span_annotations = zend_hash_str_find(Z_ARRVAL_P(span), "a", sizeof("a") - 1);

    if (span_annotations == NULL) {
        span_annotations = &span_annotations_value;
        array_init(span_annotations);
        add_assoc_zval(span, "a", span_annotations);
    }

    ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(annotations), num_key, key, zv) {
        if (key) {
            annotation_value = zval_get_string(zv);
            add_assoc_str_ex(span_annotations, ZSTR_VAL(key), ZSTR_LEN(key), annotation_value);
        }
    } ZEND_HASH_FOREACH_END();
}

void tw_span_annotate_long(long spanId, char *key, long value)
{
    zval *span, *span_annotations, span_annotations_value;
    zval annotation_value;

    if (spanId == -1) {
        return;
    }

    span = zend_hash_index_find(Z_ARRVAL(TWG(spans)), spanId);

    if (span == NULL) {
        return;
    }

    span_annotations = zend_hash_str_find(Z_ARRVAL_P(span), "a", sizeof("a") - 1);

    if (span_annotations == NULL) {
        span_annotations = &span_annotations_value;
        array_init(span_annotations);
        add_assoc_zval(span, "a", span_annotations);
    }

    ZVAL_LONG(&annotation_value, value);
    convert_to_string_ex(&annotation_value);

    add_assoc_zval_ex(span_annotations, key, strlen(key), &annotation_value);
}

void tw_span_annotate_string(long spanId, char *key, char *value, int copy)
{
    zval *span, *span_annotations, span_annotations_value;
    int key_len, value_len;
    zend_string *value_trunc;

    if (spanId == -1) {
        return;
    }

    span = zend_hash_index_find(Z_ARRVAL(TWG(spans)), spanId);

    if (span == NULL) {
        return;
    }

    span_annotations = zend_hash_str_find(Z_ARRVAL_P(span), "a", sizeof("a") - 1);

    if (span_annotations == NULL) {
        span_annotations = &span_annotations_value;
        array_init(span_annotations);
        add_assoc_zval(span, "a", span_annotations);
    }

    key_len = strlen(key);
    value_len = strlen(value);

    if (value_len < TIDEWAYS_ANNOTATION_MAX_LENGTH) {
        add_assoc_string_ex(span_annotations, key, key_len, value);
    } else {
        value_trunc = zend_string_init(value, TIDEWAYS_ANNOTATION_MAX_LENGTH, 0);
        add_assoc_str_ex(span_annotations, key, key_len, value_trunc);
    }
}
