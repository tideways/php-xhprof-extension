#include "php.h"
#include "../php_tideways.h"
#include "../spans.h"

ZEND_DECLARE_MODULE_GLOBALS(hp)

long tw_span_create(char *category, size_t category_len TSRMLS_DC)
{
	zval span, starts, stops, annotations;
	int idx;
	long parent = 0;

	if (TWG(spans) == NULL) {
		return -1;
	}

	idx = zend_hash_num_elements(Z_ARRVAL_P(TWG(spans)));

	// Hardcode a limit of 1500 spans for now, Daemon will re-filter again to 1000.
	// We assume web-requests and non-spammy worker/crons here, need a way to support
	// very long running scripts at some point.
	if (idx >= 1500) {
		return -1;
	}

	array_init(&span);
	array_init(&starts);
	array_init(&stops);
	array_init(&annotations);

	add_assoc_stringl(&span, "n", category, category_len);
	add_assoc_zval(&span, "b", &starts);
	add_assoc_zval(&span, "e", &stops);

	if (parent > 0) {
		add_assoc_long(&span, "p", parent);
	}

	zend_hash_index_update(Z_ARRVAL_P(TWG(spans)), idx, &span);

	return idx;
}

static int tw_convert_to_string(zval *zv)
{
	convert_to_string_ex(zv);

	return ZEND_HASH_APPLY_KEEP;
}

void tw_span_annotate(long spanId, zval *annotations TSRMLS_DC)
{
	zval *span, *span_annotations, span_annotations_value;

	if (spanId == -1) {
		return;
	}

	span = zend_hash_index_find(Z_ARRVAL_P(TWG(spans)), spanId);

	if (span == NULL) {
		return;
	}

	span_annotations = zend_hash_str_find(Z_ARRVAL_P(span), "a", sizeof("a") - 1);

	if (span_annotations == NULL) {
		span_annotations = &span_annotations_value;
		array_init(span_annotations);
		add_assoc_zval(span, "a", span_annotations);
	}

	zend_hash_apply(Z_ARRVAL_P(annotations), tw_convert_to_string TSRMLS_CC);

	zend_hash_merge(Z_ARRVAL_P(span_annotations), Z_ARRVAL_P(annotations), (copy_ctor_func_t) zval_add_ref, 1);
}

void tw_span_annotate_long(long spanId, char *key, long value)
{
	zval *span, *span_annotations, span_annotations_value;
	zval annotation_value;

	if (spanId == -1) {
		return;
	}

	span = zend_hash_index_find(Z_ARRVAL_P(TWG(spans)), spanId);

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

	span = zend_hash_index_find(Z_ARRVAL_P(TWG(spans)), spanId);

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

	if (value_len < 1000) {
		add_assoc_string_ex(span_annotations, key, key_len, value);
	} else {
		value_trunc = zend_string_init(value, 1000, 0);
		add_assoc_str_ex(span_annotations, key, key_len, value_trunc);
	}
}
