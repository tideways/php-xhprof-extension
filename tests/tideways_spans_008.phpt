--TEST--
Tideways: PostgreSQL Spans
--SKIPIF--
<?php
if (!extension_loaded('pgsql')) {
    print "skip: Requires pgsql extension";
}
?>
--FILE--
<?php

include_once dirname(__FILE__).'/common.php';

tideways_enable();

$conn_str = "user=postgres host=localhost port=5432";    // connection string
$db = @pg_connect($conn_str);

@pg_query($db, "select * from information_schema.tables");
@pg_query("select * from information_schema.tables");

@pg_prepare($db, "select foo", "select * from information_schema.tables");
@pg_execute($db, "select foo", array());

@pg_prepare("select bar", "select * from information_schema.tables");
@pg_execute("select bar", array());

print_spans(tideways_get_spans());
tideways_disable();
--EXPECTF--
app: 1 timers - 
sql: 1 timers - sql=select * from information_schema.tables
sql: 1 timers - sql=select * from information_schema.tables
sql: 1 timers - title=select foo
sql: 1 timers - title=select bar
