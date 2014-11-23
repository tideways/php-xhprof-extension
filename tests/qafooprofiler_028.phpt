--TEST--
XHPRof: Regression for SQL Summary
Author: beberlei
--FILE--
<?php

if (!function_exists('mysql_query')) {
    function mysql_query($sql) {}
}

qafooprofiler_enable(0, array('argument_functions' => array('mysql_query')));

@mysql_query("SHOW TABLES  FROM `foo_bar` ;");

$data = qafooprofiler_disable();

if (isset($data["main()==>mysql_query#other"])) {
    echo "SUCCESS";
} else {
    echo "FAIL";
    var_dump($data);
}
--EXPECTF--
SUCCESS
