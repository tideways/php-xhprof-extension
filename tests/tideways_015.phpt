--TEST--
XHProf: Test fatal exception handling
Author: beberlei
--FILE--
<?php

function foo() {
    try {
        throw new RuntimeException("foo");
    } catch (RuntimeException $e) {
    }
    bar();
}

function bar() {
    throw new Exception("Hello World!");
}

register_shutdown_function(function () {
    var_dump(tideways_last_fatal_error());
});

tideways_enable();

foo();
--EXPECTF--
Fatal error: Uncaught exception 'Exception' with message 'Hello World!' in %s/tideways_015.php:12
Stack trace:
#0 %s/tideways_015.php(8): bar()
#1 %s/tideways_015.php(21): foo()
#2 {main}
  thrown in %s/tideways_015.php on line 12
array(7) {
  ["message"]=>
  string(12) "Hello World!"
  ["trace"]=>
  NULL
  ["file"]=>
  string(%d) "%stests/tideways_015.php"
  ["type"]=>
  int(1)
  ["line"]=>
  int(12)
  ["class"]=>
  string(9) "Exception"
  ["code"]=>
  int(0)
}
