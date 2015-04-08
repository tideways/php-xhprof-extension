--TEST--
Tideways: exception function
--FILE--
<?php

function foo($e) {}
function bar($name) {}
function baz($other, \Exception $e, $another) {}

tideways_enable(0, array('exception_function' => 'foo'));

foo($original = new Exception("foo"));

$fetched = tideways_last_detected_exception();
tideways_disable();

var_dump($fetched->getMessage());
var_dump($fetched === $original);

tideways_enable(0, array('exception_function' => 'foo'));

foo($original = new RuntimeException("detect mode"));

$fetched = tideways_last_detected_exception();
tideways_disable();

var_dump($fetched->getMessage());

tideways_enable(0, array('exception_function' => 'baz'));
baz(1, $original = new RuntimeException("random argument"), 2);
$fetched = tideways_last_detected_exception();
tideways_disable();

var_dump($fetched->getMessage());

--EXPECTF--
string(3) "foo"
bool(true)
string(11) "detect mode"
string(15) "random argument"
