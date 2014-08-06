<?php

$options = array('argument_functions' => array('mysqli_query', 'mysqli::query'));

xhprof_enable(0, $options);

$conn = mysqli_connect('127.0.0.1', 'root', '', 'mysql');
mysqli_query($conn, 'SELECT count(*) FROM user');

$conn = new mysqli('127.0.0.1', 'root', '', 'mysql');
$conn->query('SELECT count(*) FROM user');

var_dump(xhprof_disable());
