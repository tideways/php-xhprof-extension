--TEST--
Tideways: Predis Support
--FILE--
<?php

require_once __DIR__ . '/common.php';
require_once __DIR__ . '/common_predis.php';

$client = new \Predis\Client;

tideways_enable();
$client->hexists("foo");
$client->hget("foo");
tideways_disable();

print_spans(tideways_get_spans());
--EXPECTF--
app: 1 timers - cpu=%d
predis: 1 timers - title=hexists
predis: 1 timers - title=hget
