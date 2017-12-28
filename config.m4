PHP_ARG_ENABLE(tideways_xhprof, whether to enable tideways_xhprof support, [  --enable-tideways-xhprof           Enable tideways-xhprof support])

if test "$PHP_HELLOWORLD" != "no"; then
    LIBS="$LIBS -lrt"
    LDFLAGS="$LDFLAGS -lrt"

    AC_CHECK_FUNCS(gettimeofday)
    AC_CHECK_FUNCS(clock_gettime)

    PHP_NEW_EXTENSION(tideways_xhprof, tideways_xhprof.c tracing.c, $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
fi
