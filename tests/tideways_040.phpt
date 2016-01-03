--TEST--
Tideways: Filtered Functions (PHP 7)
--SKIPIF--
<?php
if (PHP_VERSION_ID < 70000) echo "skip: PHP 7 required\n";
--FILE--
<?php

include_once dirname(__FILE__).'/common.php';

function my_func() {
    return substr("foo", 0, 1);
}

function my_other() {
    my_func();
}

tideways_enable(TIDEWAYS_FLAGS_NO_SPANS, array('ignored_functions' => array('my_func')));

my_func();
my_other();

call_user_func('my_func');
call_user_func('my_other');

$output = tideways_disable();
echo "With ignored_functions:\n";
print_canonical($output);

tideways_enable();
my_func();
my_other();

call_user_func('my_func');
call_user_func('my_other');

$output = tideways_disable();
echo "\nWithout ignored_functions:\n";
print_canonical($output);
--EXPECTF--
With ignored_functions:
main()                                  : ct=       1; wt=*;
main()==>my_other                       : ct=       2; wt=*;
main()==>substr                         : ct=       2; wt=*;
main()==>tideways_disable               : ct=       1; wt=*;
my_other==>substr                       : ct=       2; wt=*;

Without ignored_functions:
main()                                  : ct=       1; wt=*;
main()==>my_func                        : ct=       2; wt=*;
main()==>my_other                       : ct=       2; wt=*;
main()==>tideways_disable               : ct=       1; wt=*;
my_func==>substr                        : ct=       4; wt=*;
my_other==>my_func                      : ct=       2; wt=*;
