--TEST--
Tideways: Span Callback no Leaks
--FILE--
<?php
function foo() {}
function foo_cb() {}

tideways_enable();
tideways_span_callback('foo', 'foo_cb');
foo();
tideways_disable();
--EXPECTF--
