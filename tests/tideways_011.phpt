--TEST--
XHProf: Stream Summary
--FILE--
<?php

include_once dirname(__FILE__).'/common.php';

$options = array('argument_functions' => array('file_get_contents'));

tideways_enable(0, $options);

file_get_contents(__FILE__);
file_get_contents("http://localhost:80");

print_canonical(tideways_disable());
--EXPECTF--
main()                                  : ct=       1; wt=*;
main()==>file_get_contents#             : ct=       1; wt=*;
main()==>file_get_contents#http://localhost:80: ct=       1; wt=*;
main()==>tideways_disable               : ct=       1; wt=*;
