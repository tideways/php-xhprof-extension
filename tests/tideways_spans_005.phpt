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

tideways_enable();

$pdo = new PDO('sqlite:memory:', 'root', '');

$pdo->exec("CREATE TABLE baz (id INTEGER)");

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

print_spans(tideways_get_spans());
tideways_disable();
--EXPECTF--
sql: 1 timers - title=other
sql: 1 timers - title=select 'baz'
sql: 1 timers - title=insert baz
sql: 1 timers - title=update baz
sql: 1 timers - title=delete baz
sql: 2 timers - title=select baz
