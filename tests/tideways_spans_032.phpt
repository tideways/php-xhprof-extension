--TEST--
Tideways: Mysql support
--SKIPIF--
<?php
if(!extension_loaded('mysql')) {
    echo "skip: mysqli not installed\n";
    exit();
}
if (!fsockopen('127.0.0.1', 3306)) {
    echo "skip: no MySQL running on localhost:3306\n";
    exit();
}
--FILE--
<?php

include __DIR__ . "/common.php";

tideways_enable();

$link = @mysql_connect('127.0.0.1', 'root', '');

mysql_query('SELECT * FROM TABLES LIMIT 1', $link);
mysql_close($link);

print_spans(tideways_get_spans());
tideways_disable();
--EXPECTF--
app: 1 timers - 
sql: 1 timers - db.type=mysql peer.host=127.0.0.1
sql: 1 timers - sql=SELECT * FROM TABLES LIMIT 1

