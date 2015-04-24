--TEST--
Tideways: Skip require/include and eval profiling
--FILE--
<?php

tideways_enable(TIDEWAYS_FLAGS_NO_COMPILE);

require_once dirname(__FILE__) . "/common.php";
include dirname(__FILE__) . "/tideways_004_inc.php";

eval("function evaledfoo() {}");

evaledfoo();

$output = tideways_disable();
print_canonical($output);
echo "\n";
--EXPECTF--
abc,def,ghi
I am in foo()...
main()                                  : ct=       1; wt=*;
main()==>dirname                        : ct=       2; wt=*;
main()==>evaledfoo                      : ct=       1; wt=*;
main()==>explode                        : ct=       1; wt=*;
main()==>foo                            : ct=       1; wt=*;
main()==>implode                        : ct=       1; wt=*;
main()==>tideways_disable               : ct=       1; wt=*;
