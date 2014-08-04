<?php

echo phpversion('xhprof') . "\n";
xhprof_enable(0, array('argument_functions' => array('PDOStatement::execute', 'PDO::query')));

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

var_dump(xhprof_disable());
