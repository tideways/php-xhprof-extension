--TEST--
XHPRof: Check for Garbage Collection overhead
Author: beberlei
--FILE--
<?php

function create_garbage()
{
    for ($i = 0; $i < 11000; $i++) {
        $a = new stdClass;
        $a->self = $a;
    }
    gc_collect_cycles();
}

qafooprofiler_enable();
create_garbage();
$data = qafooprofiler_disable();

echo "Garbage Collection runs: " . $data['main()']['gc'] . "\n";
echo "Garbage Collection Cycles Collected: " . $data['main()']['gcc'] . "\n";

--EXPECTF--
Garbage Collection runs: 2
Garbage Collection Cycles Collected: 10999
