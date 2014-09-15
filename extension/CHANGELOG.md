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
