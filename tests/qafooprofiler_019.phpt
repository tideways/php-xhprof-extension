--TEST--
XHPRof: qafooprofiler_layers_enable()
Author: beberlei
--FILE--
<?php

include_once dirname(__FILE__).'/common.php';

function foo($x) {
    return file_get_contents(__FILE__);
}
function bar($x) {
    return strlen($x);
}

echo "With layers:\n";
qafooprofiler_layers_enable(array('file_get_contents' => 'io', 'strlen' => 'db', 'main()' => 'main()'));
foo("bar");
bar("baz");
$data = qafooprofiler_disable();

print_canonical($data);

echo "\nWithout layers:\n";
qafooprofiler_layers_enable(NULL);
?>
--EXPECTF--
With layers:
db                                      : ct=       1; wt=*;
io                                      : ct=       1; wt=*;
main()                                  : ct=       1; wt=*;

Without layers:

Notice: qafooprofiler_layers_enable() requires first argument to be array in %s/qafooprofiler_019.php on line %d
