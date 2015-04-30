--TEST--
Tideways: All Userland Depth
--FILE--
<?php

include __DIR__ . "/common.php";

function boing() {
    strlen("foo");
}

function baz() {
    boing();
    strlen("foo");
}

function bar() {
    baz();
    strlen("foo");
}

function foo() {
    bar();
    strlen("foo");
}

echo "3 layers\n";
tideways_enable(0, array('all_userland_depth' => 3));
foo();
tideways_disable();
print_spans(tideways_get_spans());
echo "\n";

echo "1 layer\n";
tideways_enable(0, array('all_userland_depth' => 1));
foo();
tideways_disable();
print_spans(tideways_get_spans());
--EXPECTF--
3 layers
app: 1 timers - 
php: 1 timers - title=baz
php: 1 timers - title=bar
php: 1 timers - title=foo

1 layer
app: 1 timers - 
php: 1 timers - title=foo
