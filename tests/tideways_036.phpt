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
main()                                  : cpu=*; ct=       1; mu=*; pmu=*; wt=*;
main()==>dirname                        : cpu=*; ct=       2; mu=*; pmu=*; wt=*;
main()==>evaledfoo                      : cpu=*; ct=       1; mu=*; pmu=*; wt=*;
main()==>run_init::tests/common.php     : cpu=*; ct=       1; mu=*; pmu=*; wt=*;
main()==>run_init::tests/tideways_004_inc.php: cpu=*; ct=       1; mu=*; pmu=*; wt=*;
main()==>tideways_disable               : cpu=*; ct=       1; mu=*; pmu=*; wt=*;
run_init::tests/tideways_004_inc.php==>explode: cpu=*; ct=       1; mu=*; pmu=*; wt=*;
run_init::tests/tideways_004_inc.php==>foo: cpu=*; ct=       1; mu=*; pmu=*; wt=*;
run_init::tests/tideways_004_inc.php==>implode: cpu=*; ct=       1; mu=*; pmu=*; wt=*;
