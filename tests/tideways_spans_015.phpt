--TEST--
Tideways: Watch Dynamic Callback
--FILE--
<?php

include __DIR__ . '/common.php';

function foo() {
}

tideways_enable();
tideways_span_watch("foo");

foo();

print_spans(tideways_get_spans());
--EXPECTF--
app: 1 timers - 
php: 1 timers - title=foo
