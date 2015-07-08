--TEST--
Tideways: Watch Dynamic Callback
--FILE--
<?php

include __DIR__ . '/common.php';

function foo() {
}
function dispatchEvent($name) {
    usleep(101);
}
function renderView($script) {
}

tideways_enable();
tideways_span_watch("foo");
tideways_span_watch("dispatchEvent", "event");
tideways_span_watch("renderView", "view");

foo();
dispatchEvent("ev1");
renderView("/var/www/foo/bar/baz.html");

print_spans(tideways_get_spans());
--EXPECTF--
app: 1 timers - 
php: 1 timers - title=foo
event: 1 timers - title=ev1
view: 1 timers - title=bar/baz.html
