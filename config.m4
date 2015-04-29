
PHP_ARG_ENABLE(tideways, whether to enable Qafoo Profiler support,
[ --enable-qafoo-profiler      Enable Qafoo Profiler support])

if test "$PHP_TIDEWAYS" != "no"; then
  PHP_SUBST([TIDEWAYS_SHARED_LIBADD])
  PHP_NEW_EXTENSION(tideways, tideways.c, $ext_shared)
fi
