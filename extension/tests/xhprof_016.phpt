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

for ($i = 0; $i < 10; $i++) {
    foo();
}
bar();

$result = xhprof_disable();
print_canonical($result['main()']['layers']);
--EXPECT--
db                                      : ct=      10; wt=*;
io                                      : ct=       1; wt=*;
