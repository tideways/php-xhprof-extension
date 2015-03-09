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

qafooprofiler_enable(0, array('argument_functions' => array('pg_query')));

$conn_str = "user=postgres host=localhost port=5432";    // connection string
$db = pg_connect($conn_str);

pg_query($db, "select * from information_schema.tables");

print_canonical(qafooprofiler_disable());

--EXPECTF--
main()                                  : ct=       1; wt=*;
main()==>pg_connect                     : ct=       1; wt=*;
main()==>pg_query#select information_schema.tables: ct=       1; wt=*;
main()==>qafooprofiler_disable          : ct=       1; wt=*;
