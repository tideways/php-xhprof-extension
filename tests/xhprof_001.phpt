--TEST--
Tideways: Basic Profiling Test
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
$output = tideways_xhprof_disable();

echo "Part 1: Default Flags\n";
print_canonical($output);
echo "\n";

// 2: Sanity test profiling options
tideways_xhprof_enable(TIDEWAYS_XHPROF_FLAGS_CPU);
foo("this is a test");
$output = tideways_xhprof_disable();

echo "Part 2: CPU\n";
print_canonical($output);
echo "\n";

// 3: Sanity test profiling options
tideways_xhprof_enable(TIDEWAYS_XHPROF_FLAGS_NO_BUILTINS);
foo("this is a test");
$output = tideways_xhprof_disable();

echo "Part 3: No Builtins\n";
print_canonical($output);
echo "\n";

// 4: Sanity test profiling options
tideways_xhprof_enable(TIDEWAYS_XHPROF_FLAGS_MEMORY);
foo("this is a test");
$output = tideways_xhprof_disable();

echo "Part 4: Memory\n";
print_canonical($output);
echo "\n";

// 5: Sanity test combo of profiling options
tideways_xhprof_enable(TIDEWAYS_XHPROF_FLAGS_MEMORY + TIDEWAYS_XHPROF_FLAGS_CPU);
foo("this is a test");
$output = tideways_xhprof_disable();

echo "Part 5: Memory & CPU\n";
print_canonical($output);
echo "\n";

// 6: Sanity test extended memory profiling
tideways_xhprof_enable(TIDEWAYS_XHPROF_FLAGS_MEMORY_ALLOC);
foo("this is a test");
$output = tideways_xhprof_disable();

echo "Part 6: Extended Memory Profiling\n";
print_canonical($output);
echo "\n";

// 7: Sanity test extended memory profiling as mu
tideways_xhprof_enable(TIDEWAYS_XHPROF_FLAGS_MEMORY_ALLOC_AS_MU);
foo("this is a test");
$output = tideways_xhprof_disable();

echo "Part 7: Extended Memory Profiling as mu\n";
print_canonical($output);
echo "\n";

?>
--EXPECT--
Part 1: Default Flags
foo==>bar                               : ct=       2; wt=*;
main()                                  : ct=       1; wt=*;
main()==>foo                            : ct=       1; wt=*;
main()==>tideways_xhprof_disable        : ct=       1; wt=*;

Part 2: CPU
foo==>bar                               : cpu=*; ct=       2; wt=*;
main()                                  : cpu=*; ct=       1; wt=*;
main()==>foo                            : cpu=*; ct=       1; wt=*;
main()==>tideways_xhprof_disable        : cpu=*; ct=       1; wt=*;

Part 3: No Builtins
foo==>bar                               : ct=       2; wt=*;
main()                                  : ct=       1; wt=*;
main()==>foo                            : ct=       1; wt=*;

Part 4: Memory
foo==>bar                               : ct=       2; mu=*; pmu=*; wt=*;
main()                                  : ct=       1; mu=*; pmu=*; wt=*;
main()==>foo                            : ct=       1; mu=*; pmu=*; wt=*;
main()==>tideways_xhprof_disable        : ct=       1; mu=*; pmu=*; wt=*;

Part 5: Memory & CPU
foo==>bar                               : cpu=*; ct=       2; mu=*; pmu=*; wt=*;
main()                                  : cpu=*; ct=       1; mu=*; pmu=*; wt=*;
main()==>foo                            : cpu=*; ct=       1; mu=*; pmu=*; wt=*;
main()==>tideways_xhprof_disable        : cpu=*; ct=       1; mu=*; pmu=*; wt=*;

Part 6: Extended Memory Profiling
foo==>bar                               : ct=       2; mem.aa=*; mem.na=*; mem.nf=*; wt=*;
main()                                  : ct=       1; mem.aa=*; mem.na=*; mem.nf=*; wt=*;
main()==>foo                            : ct=       1; mem.aa=*; mem.na=*; mem.nf=*; wt=*;
main()==>tideways_xhprof_disable        : ct=       1; mem.aa=*; mem.na=*; mem.nf=*; wt=*;

Part 7: Extended Memory Profiling as mu
foo==>bar                               : ct=       2; mem.aa=*; mem.na=*; mem.nf=*; mu=*; wt=*;
main()                                  : ct=       1; mem.aa=*; mem.na=*; mem.nf=*; mu=*; wt=*;
main()==>foo                            : ct=       1; mem.aa=*; mem.na=*; mem.nf=*; mu=*; wt=*;
main()==>tideways_xhprof_disable        : ct=       1; mem.aa=*; mem.na=*; mem.nf=*; mu=*; wt=*;
