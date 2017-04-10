#ifndef TIDEWAYS_SPANS_H
#define TIDEWAYS_SPANS_H
long tw_span_create(char *category, size_t category_len TSRMLS_DC);
void tw_span_annotate(long spanId, zval *annotations TSRMLS_DC);
void tw_span_annotate_long(long spanId, char *key, long value TSRMLS_DC);
void tw_span_annotate_string(long spanId, char *key, char *value, int copy TSRMLS_DC);

#ifndef ZVAL_COPY_VALUE
#define ZVAL_COPY_VALUE(z, v)              \
        do {                               \
                (z)->value = (v)->value;   \
                Z_TYPE_P(z) = Z_TYPE_P(v); \
        } while (0)
#endif
#endif
