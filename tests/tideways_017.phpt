--TEST--
XHPRof: Test multiple argument_functions calls
Author: beberlei
--FILE--
<?php

include_once dirname(__FILE__).'/common.php';

tideways_enable(0, array('argument_functions' => array('strlen')));

strlen("foo");
strlen("bar");

$result = tideways_disable();

tideways_enable(0, array('argument_functions' => array('strlen')));

strlen("foo");
strlen("bar");

print_canonical(tideways_disable());

tideways_enable(0, array('argument_functions' => array('strlen')));
$result = tideways_disable();

?>
--EXPECT--
main()                                  : ct=       1; wt=*;
main()==>strlen#bar                     : ct=       1; wt=*;
main()==>strlen#foo                     : ct=       1; wt=*;
main()==>tideways_disable               : ct=       1; wt=*;
