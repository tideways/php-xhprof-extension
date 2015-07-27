--TEST--
Tideways: Exclude userland functions from profilling
--FILE--
<?php

include_once dirname(__FILE__).'/common.php';

function foo($x) {
    return bar($x);
}
function bar($x) {
    return substr($x, 0, 1);
}

tideways_enable(TIDEWAYS_FLAGS_NO_USERLAND);
foo("foo");
$data = tideways_disable();

echo "Output:\n";
print_canonical($data);
?>
--EXPECTF--
Output:
main()                                  : ct=       1; wt=*;
main()==>substr                         : ct=       1; wt=*;
main()==>tideways_disable               : ct=       1; wt=*;
