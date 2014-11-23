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
