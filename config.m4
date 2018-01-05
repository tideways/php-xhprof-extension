PHP_ARG_ENABLE(tideways_xhprof, whether to enable tideways_xhprof support, [  --enable-tideways-xhprof           Enable tideways-xhprof support])

AC_DEFUN([AC_TIDEWAYS_CLOCK],
[
  have_clock_gettime=no

  AC_MSG_CHECKING([for clock_gettime])

  AC_TRY_LINK([ #include <time.h> ], [struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);], [
    have_clock_gettime=yes
    AC_MSG_RESULT([yes])
  ], [
    AC_MSG_RESULT([no])
  ])

  if test "$have_clock_gettime" = "no"; then
    AC_MSG_CHECKING([for clock_gettime in -lrt])

    SAVED_LIBS="$LIBS"
    LIBS="$LIBS -lrt"

    AC_TRY_LINK([ #include <time.h> ], [struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);], [
      have_clock_gettime=yes
      TIDEWAYS_SHARED_LIBADD="$TIDEWAYS_SHARED_LIBADD -lrt"
      AC_MSG_RESULT([yes])
    ], [
      LIBS="$SAVED_LIBS"
      AC_MSG_RESULT([no])
    ])
  fi

  if test "$have_clock_gettime" = "no"; then
    AC_MSG_CHECKING([for clock_get_time])

    AC_TRY_RUN([ #include <mach/mach.h>
      #include <mach/clock.h>
      #include <mach/mach_error.h>

      int main()
      {
        kern_return_t ret; clock_serv_t aClock; mach_timespec_t aTime;
        ret = host_get_clock_service(mach_host_self(), REALTIME_CLOCK, &aClock);

        if (ret != KERN_SUCCESS) {
          return 1;
        }

        ret = clock_get_time(aClock, &aTime);
        if (ret != KERN_SUCCESS) {
          return 2;
        }

        return 0;
      }
    ], [
      have_clock_gettime=yes
      AC_DEFINE([HAVE_CLOCK_GET_TIME], 1, [do we have clock_get_time?])
      AC_MSG_RESULT([yes])
    ], [
      AC_MSG_RESULT([no])
    ])
  fi

  if test "$have_clock_gettime" = "yes"; then
      AC_DEFINE([HAVE_CLOCK_GETTIME], 1, [do we have clock_gettime?])
  fi

  if test "$have_clock_gettime" = "no"; then
      AC_MSG_ERROR([clock_gettime is missing, but required])
  fi
])

if test "$PHP_TIDEWAYS_XHPROF" != "no"; then

    PHP_ARG_ENABLE(developer-flags, whether to enable developer build flags,
        [  --enable-developer-flags   Enable developer flags],, no)

    if test "$PHP_DEVELOPER_FLAGS" = "yes"; then
        dnl Warn about functions which might be candidates for format attributes
        PHP_CHECK_GCC_ARG(-Wmissing-format-attribute,       _MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS -Wmissing-format-attribute")
        dnl Avoid duplicating values for an enum
        PHP_CHECK_GCC_ARG(-Wduplicate-enum,                 _MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS -Wduplicate-enum")
        dnl Warns on mismatches between #ifndef and #define header guards
        PHP_CHECK_GCC_ARG(-Wheader-guard,                   _MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS -Wheader-guard")
        dnl logical not of a non-boolean expression
        PHP_CHECK_GCC_ARG(-Wlogical-not-parentheses,        _MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS -Wlogical-not-parentheses")
        dnl Warn about suspicious uses of logical operators in expressions
        PHP_CHECK_GCC_ARG(-Wlogical-op,                     _MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS -Wlogical-op")
        dnl memory error detector.
        dnl FIXME: -fsanitize=address,undefined for clang. The PHP_CHECK_GCC_ARG macro isn't happy about that string :(
        PHP_CHECK_GCC_ARG(-fsanitize-address,               _MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS -fsanitize-address")
        dnl Enable frame debugging
        PHP_CHECK_GCC_ARG(-fno-omit-frame-pointer,          _MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS -fno-omit-frame-pointer")
        dnl Make sure we don't optimize calls
        PHP_CHECK_GCC_ARG(-fno-optimize-sibling-calls,      _MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS -fno-optimize-sibling-calls")
        PHP_CHECK_GCC_ARG(-Wlogical-op-parentheses,         _MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS -Wlogical-op-parentheses")
        PHP_CHECK_GCC_ARG(-Wbool-conversion,                _MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS -Wbool-conversion")
        PHP_CHECK_GCC_ARG(-Wloop-analysis,                  _MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS -Wloop-analysis")
        PHP_CHECK_GCC_ARG(-Wsizeof-array-argument,          _MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS -Wsizeof-array-argument")
        PHP_CHECK_GCC_ARG(-Wstring-conversion,              _MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS -Wstring-conversion")
        PHP_CHECK_GCC_ARG(-Wno-variadic-macros,             _MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS -Wno-variadic-macros")
        PHP_CHECK_GCC_ARG(-Wno-sign-compare,                _MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS -Wno-sign-compare")
        PHP_CHECK_GCC_ARG(-fstack-protector,                _MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS -fstack-protector")
        PHP_CHECK_GCC_ARG(-fno-exceptions,                  _MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS -fno-exceptions")
        PHP_CHECK_GCC_ARG(-Wformat-security,                _MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS -Wformat-security")
        PHP_CHECK_GCC_ARG(-Wformat-nonliteral,              _MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS -Wformat-nonliteral")
        PHP_CHECK_GCC_ARG(-Winit-self,                      _MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS -Winit-self")
        PHP_CHECK_GCC_ARG(-Wwrite-strings,                  _MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS -Wwrite-strings")
        PHP_CHECK_GCC_ARG(-Wenum-compare,                   _MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS -Wenum-compare")
        PHP_CHECK_GCC_ARG(-Wempty-body,                     _MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS -Wempty-body")
        PHP_CHECK_GCC_ARG(-Wparentheses,                    _MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS -Wparentheses")
        PHP_CHECK_GCC_ARG(-Wdeclaration-after-statement,    _MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS -Wdeclaration-after-statement")
        PHP_CHECK_GCC_ARG(-Wmaybe-uninitialized,            _MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS -Wmaybe-uninitialized")
        PHP_CHECK_GCC_ARG(-Werror,                          _MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS -Werror")
        PHP_CHECK_GCC_ARG(-Wextra,                          _MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS -Wextra")
        PHP_CHECK_GCC_ARG(-Wno-unused-parameter,            _MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS -Wno-unused-parameter")
        PHP_CHECK_GCC_ARG(-Wno-unused-but-set-variable,     _MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS -Wno-unused-but-set-variable")
        PHP_CHECK_GCC_ARG(-Wno-missing-field-initializers,  _MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS -Wno-missing-field-initializers")

        MAINTAINER_CFLAGS="$_MAINTAINER_CFLAGS"
        STD_CFLAGS="-g -O0 -Wall"
    fi

    AC_CHECK_FUNCS(gettimeofday)
    AC_CHECK_FUNCS(clock_gettime)
    AC_TIDEWAYS_CLOCK

    PHP_TIDEWAYS_XHPROF_CFLAGS="-DZEND_ENABLE_STATIC_TSRMLS_CACHE=1 $MAINTAINER_CFLAGS $STD_CFLAGS"

    PHP_SUBST([LIBS])
    PHP_NEW_EXTENSION(tideways_xhprof, tideways_xhprof.c tracing.c, $ext_shared,, $PHP_TIDEWAYS_XHPROF_CFLAGS )
fi
