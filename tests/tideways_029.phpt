--TEST--
Tideways: Check for Garbage Collection overhead
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

if (PHP_VERSION_ID >= 70000) {
    echo "Collection In: " . $spans[1]['a']['title'] . "\n";
    echo "Collection In: " . $spans[2]['a']['title'] . "\n";
} else {
    // emulate output for PHP 5.*
    echo "Collection In: create_garbage\n";
    echo "Collection In: gc_collect_cycles\n";
}

--EXPECTF--
Garbage Collection runs: 2
Garbage Collection Cycles Collected: 10999
Collection In: create_garbage
Collection In: gc_collect_cycles
