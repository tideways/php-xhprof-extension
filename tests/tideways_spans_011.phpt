--TEST--
Tideways: Log any slow PHP calls
--FILE--
<?php

include __DIR__ . '/common.php';

tideways_enable();

usleep(60000);

print_spans(tideways_get_spans());
tideways_disable();
--EXPECT--
php: 1 timers - title=usleep
