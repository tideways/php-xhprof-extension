--TEST--
XHProf: Test fatal error handling
Author: beberlei
--FILE--
<?php

function foo() {
    bar();
}

function bar() {
    unknown();
}

register_shutdown_function(function () {
    var_dump(xhprof_last_fatal_error());
});

xhprof_last_fatal_error();

xhprof_enable();

foo();
--EXPECTF--
Fatal error: Call to undefined function unknown() in %s on line 8
array(5) {
  ["line"]=>
  int(8)
  ["file"]=>
  string(%d) "%s/xhprof_014.php"
  ["type"]=>
  int(1)
  ["message"]=>
  string(36) "Call to undefined function unknown()"
  ["trace"]=>
  string(%d) "#0 %s/xhprof_014.php(4): bar()
#1 %s/xhprof_014.php(19): foo()
#2 {main}"
}
