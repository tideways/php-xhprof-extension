--TEST--
XHProf: eval()
--FILE--
<?php

include_once dirname(__FILE__).'/common.php';

tideways_enable();

eval("strlen('Hello World!');");

$data = tideways_disable();
print_canonical($data);
--EXPECTF--
main()                                  : ct=       1; wt=*;
main()==>eval::tests/tideways_026.php(7) : eval()'d code: ct=       1; wt=*;
main()==>strlen                         : ct=       1; wt=*;
main()==>tideways_disable               : ct=       1; wt=*;
