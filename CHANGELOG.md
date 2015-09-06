# Version 3.0.0

- Remove SQL summarization, always keep full SQL and delegate summary
  generation to the daemon.

- Change `tideways_minify_sql()` to always return empty string, because summary
  function was removed. To pass full sql and let the daemon summarize it, create
  a span of category ``sql`` and pass an annotation with key ``sql``.

- Add Pheanstalk v2 and v3 support

- Add PhpAmqpLib support

- Add MongoDB support (Queries on MongoCollection, MongoCursor, MongoCommandCursor)

- Add Predis support

# Version 2.0.10

- Fix bug in CentOS compilation, where -lrt flag is required to use ``clock_gettime`` function.
  Research suggests this is required on some other Linux distributions as well.

# Version 2.0.9

- Remove slow_php_treshold functionality that recorded arbitrary php spans as long
  as they were 50ms and slower. This could lead to unpredictable results which are
  hard to render in the UI.

  Also fixes a potential segfault when triggered inside a generator, where parts
  of execute_data are already NULL.

# Version 2.0.8

- Always measure CPU time of the main() span.
- Cleanup CPU timer code to use `CLOCK_PROCESS_CPUTIME_ID`.
- Increase default sample-rate configuration to 30% 
- Multiple calls to tideways_enable() restart profiling instead of continuing
- Fix Doctrine span watchers for ORM 2.5

# Version 2.0.7

- Fix Apple Build

# Version 2.0.5

- Introduce `tideways_span_callback()` function that takes a function and a
  callback to start a span.

# Version 2.0.4

- Fix segfault in `tideways_span_watch()`

# Versoin 2.0.3

- Replace TSC based profiling with clock_gettime(CLOCK_GETMONOTONIC) to
  avoid binding process to CPUs.

- Add more special support for Symfony EventDispatcher

- Add Laravel support

# Version 2.0

- Add new collection mechanism using spans (See Google Dapper paper).
  There is a large list of extensions, libraries and frameworks supported
  automatically and an API to create spans yourself.

- Added Symfony2 Support for Spans

- Added Oxid Support for Spans

- Added Shopware Support for Spans

- Added Magento Support for Spans

- Added Zend Framework 1 Support for Spans

- Added span support for mysql, mysqli, PDO and pg database extensions

- Added Doctrine2 ORM Support for Spans

- Added Smarty2, Smarty2 and Twig Template Support for spans

- Added HTTP support for spans hooking into file_get_contents, curl_exec and SoapClient

# Version 1.7.1

- Reintroduce `tideways_last_fatal_error` as alias of `error_get_last()` for
  backwards compatibility reasons because Profiler PHP library is written in a
  way where extension update could break application.

# Version 1.7.0

- Remove `tideways_last_fatal_error` and `tideways_last_exception`.

- Add new method `tideways_fatal_backtrace()` that returns the data
  from `debug_backtrace()` as an array instead of string as before.
  This is easier to maintain, but requires userland code to convert to string.

- Add `"exception_function"` option that allows setting a function
  which gets passed exceptions at the framework level. You
  have access to this exception calling `tideways_last_detected_exception()`.

# Version 1.6.2

- Fix wrong definition of TIDEWAYS_FLAGS_NO_COMPILE flag.

# Version 1.6.1

- Fix segfault in `auto_prepend_library` cleanup handling.
- Add new constant `TIDEWAYS_FLAGS_NO_COMPILE` that skips
  profiling of require/include and eval statements.

# Version 1.6.0

- Move away from requireing file in module RINIT to hooking into
  auto_prepend_file. We changed the INI setting `tideways.load_library`
  to be `tideways.auto_prepend_library` instead and defaults to 1.

  It will check for `Tideways.php` next to tideways.so and load that
  library by adding it to the `auto_prepend_file` PHP.INI option.

  If that is already set a new function `tideways_prepend_overwritten()`
  allows to check if we need to require the old ini_get("auto_prepend_file").

# Version 1.5.0

- Change default socket option to `/var/run/tideways/tidewaysd.sock`
- Rename INI option `tideways.transaction_function` to `tideways.framework`
- Rename from qafooprofiler to tideways

# Version 1.3.2

- Add protection against segfault in combination with XDebug < 2.2.7
  See https://github.com/xdebug/xdebug/pull/131

# Version 1.3.1

- Improve auto loading/start functionality by using better
  abstractions from Zend Engine. Replaced copied code from `spl_autoload`
  with call to `zend_execute_scripts`.

- Bugfix in sql argument summary when FROM keyword was found, but
  it was not a SELECT/INSERT/UPDATE/DELETE statement.

# Version 1.3.0

- Add support for profiling Event-based frameworks/applications
  Optionally collect the name of the event triggered as part
  of the function call for the following libraries:

    - Doctrine 2
    - Zend Framework 2
    - Symfony 2
    - Drupal
    - Wordpress
    - Magento
    - Enlight/Shopware

- Add optional support to auto start profiling and transmitting
  to [Qafoo Profiler platform](https://qafoolabs.com) by copying
  `QafooProfiler.php` next to the `qafooprofiler.so`.

# Version 1.2.2

- Fix bug in `eval()` support

# Version 1.2.0

- Fix bug in Smarty support
- Fix bug in overwrite mechanism of `zend_execute` for transacation name detection.
- Improve performance for transaction name detection when no layer data is requested.

# Version 1.0

- Rename extension to `qafooprofiler`
- Add support for transaction name detection when not fully profiling (layer-mode)

# Version 0.9.11

- Fix segfault in Twig_Template#getTemplateName instrumentation
  on PHP versions < 5.5

- Fix segfault in memory handling of fatal error callback when catching
  an exception.

- Fix missing TSRMLS_CC/DC flags and a missing TSRMLS_FETCH() to
  allow compilation on threaded systems such as Travis running
  with --enable-zts-maintainer flag.

- Enabled Travis CI

# Version 0.9.10

- Fix segfault in Twig_Template#getTemplateName instrumentation.

- Integrate curl dependency into repository cleanly to avoid
  problems with having to copy the `php_curl.h` header around.

# Version 0.9.9

- Apply patch by tstarling@php.net to fix frequency collection on linux:
    https://bugs.php.net/bug.php?id=64165

- Apply patch by github@fabian-franz.de to fix Mac timing:
    https://bugs.php.net/bug.php?id=61132

# Version 0.9.8

- Improve performance on modern CPUs by checking for invariant tsc,
  a feature that guarantees the same timer values and speed in the
  `cpuinfo` register.

  This allows xhprof to avoid the costly bind to a single CPU, which
  causes performance problems on servers with very high load. Binding
  to cpus disallows migrating the threads to another cpu and prevents
  the Kernel to adjust different loads of cpus.

# Version 0.9.7

- Remove `xhprof_sample_disable()`, use `xhprof_disable()` instead when in
  sampling mode.

- Add `xhprof_layers_enable() ` that accepts an array of key value pairs in the
  constructor containing function names to layers. Will automatically set the
  `XHPROF_FLAGS_NOUSERLAND` mode and use the passed functions as a `functions`
  whitelist. The result is a profiling report only based on grouping certain
  function calls into layers.  If you want to profile the request as well, add
  `"main()" => "main()"` as an entry.

- Add new constant `XHPROF_FLAGS_NOUSERLAND` when set will not override
  the zend_execute hook for userland functions.

- Add new function `xhprof_last_fatal_error()` that returns information
  on PHP fatal error with trace and more information than PHP core has usually.

  Overrides zend_error_cb such that it will not work when xdebug is also enabled
  (depends on the order).

# Version 0.9.6

- Add `argument_functions` feature that allows logging the arguments of a function.
  Includes special handling for a lot of interesting functions that will want this
  feature, for example `PDO::execute`.

# Version 0.9.5

- Fix segfault with PHP 5.5.9 and up
- Add new option `functions` to `xhprof_enable()` that allows whitelist profiling of functions.
