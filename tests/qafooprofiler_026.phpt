--TEST--
XHProf: eval()
--FILE--
<?php

include_once dirname(__FILE__).'/common.php';

qafooprofiler_enable();

eval("strlen('Hello World!');");

$data = qafooprofiler_disable();
print_canonical($data);
--EXPECTF--
main()                                  : ct=       1; wt=*;
main()==>eval::tests/qafooprofiler_026.php(7) : eval()'d code: ct=       1; wt=*;
main()==>qafooprofiler_disable          : ct=       1; wt=*;
main()==>strlen                         : ct=       1; wt=*;
