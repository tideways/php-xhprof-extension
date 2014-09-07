# Version 0.9.7

- Remove `xhprof_sample_disable()`, use `xhprof_disable()` instead when in
  sampling mode.

- Add `xhprof_layers_enable() ` that accepts an array of key value pairs in the
  constructor containing function names to layers.

- Add new constant `XHPROF_FLAGS_NOUSERLAND` when set will not override
  the zend_execute hook for userland functions.

# Version 0.9.6

- Add `argument_functions` feature that allows logging the arguments of a function.
  Includes special handling for a lot of interesting functions that will want this
  feature, for example `PDO::execute`.

# Version 0.9.5

- Fix segfault with PHP 5.5.9 and up
- Add new option `functions` to `xhprof_enable()` that allows whitelist profiling of functions.
