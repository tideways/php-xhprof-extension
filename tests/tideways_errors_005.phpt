--TEST--
Tideways: Fetch Error Exception on PHP 7
--SKIPIF--
<?php
if (PHP_VERSION_ID < 70000) {
    die("skip: PHP7+");
}
--FILE--
<?php

register_shutdown_function(function () {
    var_dump("shutdown", tideways_last_detected_exception()->getMessage());
});

tideways_enable();
try {
    foobar();
} catch (Error $e) {
    foobar();
}
tideways_disable();
--EXPECTF--
Fatal error: Uncaught Error: Call to undefined function foobar() in %s
Stack trace:
#0 {main}
  thrown in %s on line 11
string(8) "shutdown"
string(35) "Call to undefined function foobar()"
