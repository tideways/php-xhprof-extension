--TEST--
XHProf: Last Exception Data
--FILE--
<?php

xhprof_enable();

try {
    throw new Exception("Foo", 1234);
} catch (Exception $e) {
}

var_dump(xhprof_last_exception_data());

try {
    throw new RuntimeException("Bar", 1337);
} catch (Exception $e) {
}

var_dump(xhprof_last_exception_data());
--EXPECTF--
array(7) {
  ["message"]=>
  string(3) "Foo"
  ["trace"]=>
  NULL
  ["file"]=>
  string(68) "%stests/xhprof_022.php"
  ["type"]=>
  int(0)
  ["line"]=>
  int(6)
  ["class"]=>
  string(9) "Exception"
  ["code"]=>
  int(1234)
}
array(7) {
  ["message"]=>
  string(3) "Bar"
  ["trace"]=>
  NULL
  ["file"]=>
  string(68) "%stests/xhprof_022.php"
  ["type"]=>
  int(0)
  ["line"]=>
  int(13)
  ["class"]=>
  string(16) "RuntimeException"
  ["code"]=>
  int(1337)
}
