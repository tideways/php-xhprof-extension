--TEST--
XHProf: Transaction name detection when disabling userland
--FILE--
<?php

function get_query_template($name) {
}

// Test 1: Detection with layers
tideways_enable(
    TIDEWAYS_FLAGS_NO_USERLAND | TIDEWAYS_FLAGS_NO_BUILTINS,
    array('transaction_function' => 'get_query_template')
);

get_query_template("home");

echo "Wordpress 1: " . tideways_transaction_name() . "\n";
$data = tideways_disable();

// Test 2: Repetition to catch global state errors
tideways_enable(
    TIDEWAYS_FLAGS_NO_USERLAND | TIDEWAYS_FLAGS_NO_BUILTINS,
    array('transaction_function' => 'get_query_template')
);
get_query_template("page");

echo "Wordpress 2: " . tideways_transaction_name() . "\n";
$data = tideways_disable();

// Test 3: Without any layers defined
tideways_enable(
    TIDEWAYS_FLAGS_NO_USERLAND | TIDEWAYS_FLAGS_NO_BUILTINS,
    array('transaction_function' => 'get_query_template')
);
get_query_template("post");

echo "Wordpress 3: " . tideways_transaction_name() . "\n";
$data = tideways_disable();

--EXPECTF--
Wordpress 1: home
Wordpress 2: page
Wordpress 3: post
