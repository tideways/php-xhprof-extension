--TEST--
Tideways: PDO MySQL Connect
--SKIPIF--
<?php
if(!extension_loaded('pdo_mysql')) {
    echo "skip: pdo_mysql not installed\n";
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

$pdo = new PDO('mysql:host=127.0.0.1;dbname=mysql;port=3306', 'root', '');
$pdo->query('SELECT 1');

tideways_disable();
print_spans(tideways_get_spans());
--EXPECTF--
app: 1 timers - cpu=%d
sql: 1 timers - db.name=mysql db.type=mysql peer.host=127.0.0.1 peer.port=3306
sql: 1 timers - sql=SELECT 1
