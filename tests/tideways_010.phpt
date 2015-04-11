--TEST--
XHProf: SQL Summanry
--SKIPIF--
<?php
if (!extension_loaded('pdo_sqlite')) {
    print "skip: pdo_sqlite not installed\n";
    exit(1);
}
--FILE--
<?php

include_once dirname(__FILE__).'/common.php';

tideways_enable(0, array('argument_functions' => array('PDOStatement::execute', 'PDO::query', 'PDO::exec')));

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

print_canonical(tideways_disable());
--EXPECT--
main()                                  : ct=       1; wt=*;
main()==>PDO::__construct               : ct=       1; wt=*;
main()==>PDO::exec#other                : ct=       1; wt=*;
main()==>PDO::prepare                   : ct=       4; wt=*;
main()==>PDO::query#select baz          : ct=       1; wt=*;
main()==>PDOStatement::execute#delete baz: ct=       1; wt=*;
main()==>PDOStatement::execute#insert baz: ct=       1; wt=*;
main()==>PDOStatement::execute#select 'baz': ct=       1; wt=*;
main()==>PDOStatement::execute#select baz: ct=       1; wt=*;
main()==>PDOStatement::execute#update baz: ct=       1; wt=*;
main()==>tideways_disable               : ct=       1; wt=*;
