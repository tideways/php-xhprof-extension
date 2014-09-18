--TEST--
XHProf: Last Exception Data
--FILE--
<?php

xhprof_enable();

try {
    throw new Exception();
} catch (Exception $e) {
}

var_dump(xhprof_last_exception_data());
--EXPECTF--
