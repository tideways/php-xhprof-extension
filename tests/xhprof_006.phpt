--TEST--
xhprof: dont disable no memory leak
--INI--
tideways_xhprof.clock_use_rdtsc=1
--SKIPIF--
<?php
if (PHP_OS !== "Linux") {
    echo "skip: Requires linux\n";
}
--FILE--
<?php

include_once dirname(__FILE__).'/common.php';

var_dump(ini_get("tideways_xhprof.clock_use_rdtsc"));

ob_start();
phpinfo();
$content = ob_get_clean();
$lines = explode(PHP_EOL, $content);

foreach ($lines as $line) {
    if (strpos($line, "Clock Source =>") !== false) {
        echo $line . PHP_EOL;
    }
}

function foo($x) {
}

// 1: Sanity test a simple profile run
tideways_xhprof_enable();
foo("this is a test");
$output = tideways_xhprof_disable();

print_canonical($output);
--EXPECTF--
string(1) "1"
Clock Source => tsc
main()                                  : ct=       1; wt=*;
main()==>foo                            : ct=       1; wt=*;
main()==>tideways_xhprof_disable        : ct=       1; wt=*;
