--TEST--
XHPRrof: Testing dedicated layer profiling
Author: beberlei
--FILE--
<?php

include_once dirname(__FILE__).'/common.php';

function foo() {
}
function bar() {
    strlen("foo");
}

xhprof_enable(0, array('layers' => array('foo' => 'db', 'bar' => 'io')));

foo();
bar();

print_canonical(xhprof_disable());
--EXPECT--
