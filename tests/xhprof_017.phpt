--TEST--
XHPRof: Test multiple argument_functions calls
Author: beberlei
--FILE--
<?php

include_once dirname(__FILE__).'/common.php';

xhprof_enable(0, array('argument_functions' => array('strlen')));

strlen("foo");
strlen("bar");

$result = xhprof_disable();

xhprof_enable(0, array('argument_functions' => array('strlen')));

strlen("foo");
strlen("bar");

print_canonical(xhprof_disable());

xhprof_enable(0, array('argument_functions' => array('strlen')));
$result = xhprof_disable();

?>
--EXPECT--
main()                                  : ct=       1; wt=*;
main()==>strlen#bar                     : ct=       1; wt=*;
main()==>strlen#foo                     : ct=       1; wt=*;
main()==>xhprof_disable                 : ct=       1; wt=*;
