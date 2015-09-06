--TEST--
Tideways: Auto-Detect SQL Spans
--SKIPIF--
<?php
if (!extension_loaded('pdo_sqlite')) {
    print "skip: pdo_sqlite not installed\n";
    exit(1);
}
--FILE--
<?php

include_once dirname(__FILE__).'/common.php';

tideways_enable(0, array('slow_php_call_treshold' => 50000000));

$pdo = new PDO('sqlite:memory:', 'root', '');

$pdo->exec("CREATE TABLE baz (id INTEGER)");

$pdo->beginTransaction();

$stmt = $pdo->prepare("SELECT 'foo' FROM 'baz'");
$stmt->execute();

$stmt = $pdo->prepare("INSERT INTO baz (id) VALUES (1)");
$stmt->execute();

$stmt = $pdo->prepare("UPDATE baz SET id = 2 WHERE id = 1");
$stmt->execute();

$stmt = $pdo->prepare("DELETE FROM baz");
$stmt->execute();

$stmt = $pdo->query("SELECT count(*) FROM baz");
$stmt->execute();

$pdo->commit();

print_spans(tideways_get_spans());
tideways_disable();
--EXPECTF--
app: 1 timers - 
sql: 1 timers - sql=CREATE TABLE baz (id INTEGER)
sql: 1 timers - sql=SELECT 'foo' FROM 'baz'
sql: 1 timers - sql=INSERT INTO baz (id) VALUES (1)
sql: 1 timers - sql=UPDATE baz SET id = 2 WHERE id = 1
sql: 1 timers - sql=DELETE FROM baz
sql: 1 timers - sql=SELECT count(*) FROM baz
sql: 1 timers - sql=SELECT count(*) FROM baz
sql: 1 timers - title=commit
