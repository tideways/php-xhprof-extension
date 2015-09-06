--TEST--
Tideways: Mysqli Statement support
--SKIPIF--
<?php
if(!extension_loaded('mysqli')) {
    echo "skip: mysqli not installed\n";
    exit();
}
--FILE--
<?php

include __DIR__ . "/common.php";

tideways_enable(0, array('slow_php_call_treshold' => 50000000));

$mysql = new mysqli('127.0.0.1', 'root', '', 'information_schema');

$stmt = $mysql->prepare('SELECT * FROM TABLES LIMIT 1');
$stmt->execute();

print_spans(tideways_get_spans());
tideways_disable();
--EXPECTF--
app: 1 timers - 
sql: 1 timers - sql=SELECT * FROM TABLES LIMIT 1
sql: 1 timers - title=execute
