
PHP_ARG_ENABLE(tideways, whether to enable Qafoo Profiler support,
[ --enable-qafoo-profiler      Enable Qafoo Profiler support])

PHP_ARG_WITH([tideways-libcurl-dir], [],
[  --with-tideways-libcurl-dir[=DIR]  Qafoo Profiler: where to find libcurl], $PHP_TIDEWAYS, $PHP_TIDEWAYS)

if test "$PHP_TIDEWAYS_LIBCURL_DIR" = "no"; then
    AC_DEFINE([PHP_TIDEWAYS_HAVE_CURL], [0], [ ])
else
    AC_MSG_CHECKING([for curl/curl.h])
    CURL_DIR=

    for i in "$PHP_TIDEWAYS_LIBCURL_DIR" /usr/local /usr /opt; do
        if test -f "$i/include/curl/curl.h"; then
            CURL_DIR=$i
            break
        fi
    done

    if test "x$CURL_DIR" = "x"; then
        AC_MSG_RESULT([not found])
        AC_DEFINE([PHP_TIDEWAYS_HAVE_CURL], [0], [ ])
    else
        AC_MSG_RESULT([found in $CURL_DIR])

        AC_MSG_CHECKING([for curl-config])
        CURL_CONFIG=
        for i in "$CURL_DIR/bin/curl-config" "$CURL_DIR/curl-config" `which curl-config`; do
            if test -x "$i"; then
                CURL_CONFIG=$i
                break
            fi
        done
        if test "x$CURL_CONFIG" = "x"; then
            AC_MSG_RESULT([not found])
            AC_MSG_ERROR([could not find curl-config])
        else
            AC_MSG_RESULT([found: $CURL_CONFIG])
        fi

        AC_MSG_CHECKING([for curl/easy.h])

        if test -f "$CURL_DIR/include/curl/easy.h"; then
            AC_MSG_RESULT([found])

            PHP_ADD_INCLUDE($CURL_DIR/include)
            PHP_ADD_LIBRARY_WITH_PATH(curl, $CURL_DIR/$PHP_LIBDIR, TIDEWAYS_SHARED_LIBADD)
            PHP_EVAL_LIBLINE(`$CURL_CONFIG --libs`, TIDEWAYS_SHARED_LIBADD)

            AC_DEFINE([PHP_TIDEWAYS_HAVE_CURL], [1], [ ])
        else 
            AC_MSG_RESULT([not found])
        fi 
    fi
fi

if test "$PHP_TIDEWAYS" != "no"; then
  PHP_SUBST([TIDEWAYS_SHARED_LIBADD])
  PHP_NEW_EXTENSION(tideways, tideways.c, $ext_shared)
fi
