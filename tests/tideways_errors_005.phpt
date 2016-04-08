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
    var_dump(tideways_last_detected_exception());
});

tideways_enable();
foobar();
--EXPECTF--
