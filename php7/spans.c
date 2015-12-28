#include "php.h"
#include "../php_tideways.h"
#include "../spans.h"

ZEND_DECLARE_MODULE_GLOBALS(hp)

long tw_span_create(char *category, size_t category_len TSRMLS_DC)
{
	zval span, starts, stops, annotations;
	int idx;
	long parent = 0;

	idx = zend_hash_num_elements(TWG(spans));

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

	zend_compat_hash_index_update(TWG(spans), idx, &span);

	return idx;
}

void tw_span_annotate(long spanId, zval *annotations TSRMLS_DC)
{
	zval *span, *span_annotations;

	span = zend_compat_hash_index_find(TWG(spans), spanId);

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

	span = zend_compat_hash_index_find(TWG(spans), spanId);

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

	span = zend_compat_hash_index_find(TWG(spans), spanId);

	if (span == NULL) {
		return;
	}

	span_annotations = zend_compat_hash_find_const(Z_ARRVAL_P(span), "a", sizeof("a") - 1);

	if (span_annotations == NULL) {
		return;
	}

	_add_assoc_string_ex(span_annotations, key, strlen(key)+1, value, copy);
}
