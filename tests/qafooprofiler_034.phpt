--TEST--
XHPRof: PostgreSQL Extension
Author: beberlei
--SKIPIF--
<?php
if (!extension_loaded('pgsql')) {
    print "skip: Requires pgsql extension";
}
?>
--FILE--
<?php

include_once dirname(__FILE__).'/common.php';

qafooprofiler_enable(0, array('argument_functions' => array('pg_query', 'pg_execute')));

$conn_str = "user=postgres host=localhost port=5432";    // connection string
$db = pg_connect($conn_str);

pg_query($db, "select * from information_schema.tables");
pg_query("select * from information_schema.tables");

pg_prepare($db, "select foo", "select * from information_schema.tables");
pg_execute($db, "select foo", array());

pg_prepare("select bar", "select * from information_schema.tables");
pg_execute("select bar", array());

print_canonical(qafooprofiler_disable());

--EXPECTF--
main()                                  : ct=       1; wt=*;
main()==>pg_connect                     : ct=       1; wt=*;
main()==>pg_execute#select bar          : ct=       1; wt=*;
main()==>pg_execute#select foo          : ct=       1; wt=*;
main()==>pg_prepare                     : ct=       2; wt=*;
main()==>pg_query#select information_schema.tables: ct=       2; wt=*;
main()==>qafooprofiler_disable          : ct=       1; wt=*;
