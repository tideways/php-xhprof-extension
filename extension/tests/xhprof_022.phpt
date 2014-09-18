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
--EXPECTF--
