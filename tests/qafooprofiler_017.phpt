--TEST--
XHPRof: Test multiple argument_functions calls
Author: beberlei
--FILE--
<?php

include_once dirname(__FILE__).'/common.php';

qafooprofiler_enable(0, array('argument_functions' => array('strlen')));

strlen("foo");
strlen("bar");

$result = qafooprofiler_disable();

qafooprofiler_enable(0, array('argument_functions' => array('strlen')));

strlen("foo");
strlen("bar");

print_canonical(qafooprofiler_disable());

qafooprofiler_enable(0, array('argument_functions' => array('strlen')));
$result = qafooprofiler_disable();

?>
--EXPECT--
main()                                  : ct=       1; wt=*;
main()==>qafooprofiler_disable          : ct=       1; wt=*;
main()==>strlen#bar                     : ct=       1; wt=*;
main()==>strlen#foo                     : ct=       1; wt=*;
