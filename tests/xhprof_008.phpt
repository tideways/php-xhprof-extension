--TEST--
xhprof: internal functions calling userland functions
--FILE--
<?php

include_once dirname(__FILE__).'/common.php';

tideways_xhprof_enable();

require_once __DIR__ . "/xhprof_008.inc";

$output = tideways_xhprof_disable();

print_canonical($output);
--EXPECTF--
array_map==>double                      : ct=      10; wt=*;
main()                                  : ct=       1; wt=*;
main()==>array_map                      : ct=       1; wt=*;
main()==>range                          : ct=       1; wt=*;
main()==>tideways_xhprof_disable        : ct=       1; wt=*;
