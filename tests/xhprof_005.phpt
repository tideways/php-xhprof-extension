--TEST--
tideways: dont disable no memory leak
--FILE--
<?php

include_once dirname(__FILE__).'/common.php';

tideways_xhprof_enable();

function foo() {
}
foo();
echo "Testing";
--EXPECTF--
Testing
