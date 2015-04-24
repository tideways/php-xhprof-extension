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

tideways_enable();
create_garbage();
tideways_disable();

$spans = tideways_get_spans();

echo "Garbage Collection runs: " . $spans[0]['a']['gc'] . "\n";
echo "Garbage Collection Cycles Collected: " . $spans[0]['a']['gcc'] . "\n";

--EXPECTF--
Garbage Collection runs: 2
Garbage Collection Cycles Collected: 10999
