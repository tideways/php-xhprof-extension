--TEST--
Tideways: Transaction PHP Call detection
--FILE--
<?php

require_once __DIR__ . '/common.php';

function get_controller($name) {}
function foo() {}

tideways_enable(0, array('transaction_function' => 'get_controller'));

get_controller("foo");
foo();

echo tideways_transaction_name() . "\n";
print_spans(tideways_get_spans());
tideways_disable();
--EXPECTF--
foo
php: 1 timers - title=foo
