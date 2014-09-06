--TEST--
XHProf: Test fatal exception handling
Author: beberlei
--FILE--
<?php

function foo() {
    bar();
}

function bar() {
    throw new Exception("Hello World!");
}

register_shutdown_function(function () {
    var_dump(xhprof_last_fatal_error());
});

xhprof_enable();

foo();
--EXPECTF--
Fatal error: Uncaught exception 'Exception' with message 'Hello World!' in %s/xhprof_015.php:8
Stack trace:
#0 %s/xhprof_015.php(4): bar()
#1 %s/xhprof_015.php(17): foo()
#2 {main}
  thrown in %s/xhprof_015.php on line 8
array(5) {
  ["line"]=>
  int(8)
  ["file"]=>
  string(%d) "%s/xhprof_015.php"
  ["type"]=>
  string(9) "Exception"
  ["message"]=>
  string(12) "Hello World!"
  ["trace"]=>
  string(%d) "Uncaught exception 'Exception' with message 'Hello World!' in /home/benny/code/php/workspace/xhprof/extension/tests/xhprof_015.php:8
Stack trace:
#0 %s/xhprof_015.php(4): bar()
#1 %s/xhprof_015.php(17): foo()
#2 {main}
  thrown"
}
