--TEST--
Tideways: Disabling without start first
--FILE--
<?php

include_once dirname(__FILE__).'/common.php';


var_dump(tideways_xhprof_disable());
var_dump(tideways_xhprof_disable());
echo "done\n";
--EXPECT--
array(0) {
}
array(0) {
}
done
