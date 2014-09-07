--TEST--
XHPRof: Exclude userland functions from profilling
Author: beberlei
--FILE--
<?php

include_once dirname(__FILE__).'/common.php';

function foo($x) {
    return bar($x);
}
function bar($x) {
    return strlen($x);
}

xhprof_enable(XHPROF_FLAGS_NO_USERLAND);
foo("foo");
$data = xhprof_disable();

print_canonical($data);
?>
--EXPECTF--
main()                                  : ct=       1; wt=*;
main()==>strlen                         : ct=       1; wt=*;
main()==>xhprof_disable                 : ct=       1; wt=*;
