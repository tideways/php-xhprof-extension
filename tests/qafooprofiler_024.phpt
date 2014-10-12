--TEST--
XHProf: Transaction name detecion in layering mode
--FILE--
<?php

function get_query_template($name) {
}

qafooprofiler_layers_enable(
    array('strlen' => 'db'),
    'get_query_template'
);

get_query_template("home");

echo "Wordpress 1: " . qafooprofiler_transaction_name() . "\n";
$data = qafooprofiler_disable();

qafooprofiler_layers_enable(
    array('strlen' => 'db'),
    'get_query_template'
);
get_query_template("page");

echo "Wordpress 2: " . qafooprofiler_transaction_name() . "\n";
$data = qafooprofiler_disable();

--EXPECTF--
Wordpress 1: home
Wordpress 2: page
