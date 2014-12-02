--TEST--
XHPRof: Check for Garbage Collection overhead
Author: beberlei
--FILE--
<?php

function create_garbage()
{
    $foo = array();
    for ($i = 0; $i < 11000; $i++) {
        $foo[] = str_repeat("x", $i);
    }
    gc_collect_cycles();
    return $foo;
}

qafooprofiler_enable();
create_garbage();
var_dump(qafooprofiler_disable());

--EXPECTF--
