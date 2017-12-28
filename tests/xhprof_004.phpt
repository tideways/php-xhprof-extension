--TEST--
Tideways: Closure and Anonymous Classes
--FILE--
<?php

include_once dirname(__FILE__).'/common.php';

tideways_xhprof_enable();

$foo = function() {};
$foo();

$bar = new class () {
    public function baz() {
    }
};
$bar->baz();

$output = tideways_xhprof_disable();

print_canonical($output);

?>
--EXPECTF--
main()                                  : ct=       1; wt=*;
main()==>class@anonymous::baz           : ct=       1; wt=*;
main()==>tideways_xhprof_disable        : ct=       1; wt=*;
main()==>{closure}                      : ct=       1; wt=*;
