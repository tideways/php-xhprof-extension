--TEST--
XHProf: Transaction name detecion in layering mode
--FILE--
<?php

function get_query_template($name) {
}

// Test 1: Detection with layers
qafooprofiler_layers_enable(
    array('strlen' => 'db'),
    'get_query_template'
);

get_query_template("home");

echo "Wordpress 1: " . qafooprofiler_transaction_name() . "\n";
$data = qafooprofiler_disable();

// Test 2: Repetition to catch global state errors
qafooprofiler_layers_enable(
    array('strlen' => 'db'),
    'get_query_template'
);
get_query_template("page");

echo "Wordpress 2: " . qafooprofiler_transaction_name() . "\n";
$data = qafooprofiler_disable();

// Test 3: Without any layers defined
qafooprofiler_layers_enable(array(), 'get_query_template');
get_query_template("post");

echo "Wordpress 3: " . qafooprofiler_transaction_name() . "\n";
$data = qafooprofiler_disable();

--EXPECTF--
Wordpress 1: home
Wordpress 2: page
Wordpress 3: post
