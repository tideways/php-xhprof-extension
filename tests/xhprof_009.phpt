--TEST--
Tideways: Nested Profiling Test
--FILE--
<?php

include_once dirname(__FILE__).'/common.php';

function bar() {
  return 1;
}

function foo($x) {
  $sum = 0;
  for ($idx = 0; $idx < 2; $idx++) {
     $sum += bar();
  }
  return strlen("hello: {$x}");
}

// 1: Sanity test a simple profile run
tideways_xhprof_enable();
foo("this is a test");
tideways_xhprof_enable();
foo("this is a test");
$output = tideways_xhprof_disable();

print_canonical($output);
echo "\n";
--EXPECT--
foo==>bar                               : ct=       4; wt=*;
main()                                  : ct=       1; wt=*;
main()==>foo                            : ct=       2; wt=*;
main()==>tideways_xhprof_disable        : ct=       1; wt=*;
main()==>tideways_xhprof_enable         : ct=       1; wt=*;
