--TEST--
XHPRrof: Test excluding call_user_func and similar functions
Author: mpal
--FILE--
<?php

include_once dirname(__FILE__).'/common.php';

function foo($str) {
    return strlen($str);
}

qafooprofiler_enable(0, array('argument_functions' => array('strlen')));

foo("bar");
foo("baz");
foo("baz");

print_canonical(qafooprofiler_disable());
--EXPECT--
foo==>strlen#bar                        : ct=       1; wt=*;
foo==>strlen#baz                        : ct=       2; wt=*;
main()                                  : ct=       1; wt=*;
main()==>foo                            : ct=       3; wt=*;
main()==>qafooprofiler_disable          : ct=       1; wt=*;
