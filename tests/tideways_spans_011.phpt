--TEST--
Tideways: Log any slow PHP calls
--FILE--
<?php

include __DIR__ . '/common.php';

tideways_enable();

function foo() {
    usleep(60000);
}
foo();

print_spans(tideways_get_spans());
tideways_disable();
--EXPECT--
app: 1 timers - 
php: 1 timers - title=usleep
