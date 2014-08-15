<?php

$options = array('argument_functions' => array('mysql_query'));

xhprof_enable(0, $options);

$conn = mysql_connect('127.0.0.1', 'root', '');
mysql_query('USE mysql');
mysql_query('SELECT count(*) FROM user');

var_dump(xhprof_disable());
