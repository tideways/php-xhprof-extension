--TEST--
Tideways: APC/APCu Support
--FILE--
<?php

require_once __DIR__ . '/common.php';

if (!function_exists('apcu_store')) {
    function apcu_store() {}
    function apcu_fetch() {}
}

tideways_enable();

apcu_fetch("foo");
apcu_store("foo", 1234);

tideways_disable();

print_spans(tideways_get_spans());
--EXPECTF--
app: 1 timers - cpu=%d
apc: 1 timers - title=apcu_fetch
apc: 1 timers - title=apcu_store
