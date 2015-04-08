--TEST--
Tideways: tideways_last_fatal_error() for B/C reasons
--FILE--
<?php

register_shutdown_function(function() {
    var_dump(tideways_last_fatal_error());
});

foo();
--EXPECTF--
Fatal error: Call to undefined function foo() in %s/tests/tideways_errors_003.php on line 7
array(4) {
  ["type"]=>
  int(1)
  ["message"]=>
  string(32) "Call to undefined function foo()"
  ["file"]=>
  string(%d) "%s/tests/tideways_errors_003.php"
  ["line"]=>
  int(7)
}
