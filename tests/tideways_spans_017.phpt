--TEST--
Tideways: Watch with Callback
--FILE--
<?php

function foo_cb() {
    var_dump("inside watcher");
    return tideways_span_create('doctrine');
}

function foo() {
}

tideways_enable();
tideways_callback_watch('foo', 'foo_cb');

foo();

var_dump(tideways_get_spans());
--EXPECTF--
