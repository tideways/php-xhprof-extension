--TEST--
Tideways: Span Callback no Leaks
--FILE--
<?php
function foo($x, $y) {}
function foo_cb() {}

tideways_enable();
tideways_span_callback('foo', 'foo_cb');
tideways_disable();
--EXPECTF--
