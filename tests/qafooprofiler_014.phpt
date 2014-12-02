--TEST--
XHProf: Test fatal error handling
--SKIPIF--
<?php
if (version_compare(PHP_VERSION, "5.4.0") < 0) {
    print "skip: Fatal error handling missing the trace on PHP 5.3";
}
?>
--FILE--
<?php

function foo() {
    bar();
}

function bar() {
    unknown();
}

register_shutdown_function(function () {
    var_dump(qafooprofiler_last_fatal_error());
});

qafooprofiler_last_fatal_error();

qafooprofiler_enable();

foo();
--EXPECTF--
Fatal error: Call to undefined function unknown() in %s on line 8
array(5) {
  ["message"]=>
  string(36) "Call to undefined function unknown()"
  ["trace"]=>
  string(%d) "#0 %s/qafooprofiler_014.php(4): bar()
#1 %s/qafooprofiler_014.php(19): foo()
#2 {main}"
  ["file"]=>
  string(%d) "%s/qafooprofiler_014.php"
  ["type"]=>
  int(1)
  ["line"]=>
  int(8)
}
