--TEST--
Tideways: call_user_func_array
--FILE--
<?php

include_once dirname(__FILE__).'/common.php';

function foo ($x) {
    return $x * $x;
}

$counter = new class {
    public function __invoke ($x) {
        return $x * $x;
    }
};

tideways_xhprof_enable();
call_user_func("foo", 2);
call_user_func_array("foo", [2]);
call_user_func_array(function ($x) {
    return $x * $x;
}, [2]);
call_user_func_array($counter, [2]);
$output = tideways_xhprof_disable();

print_canonical($output);
--EXPECTF--
main()                                  : ct=       1; wt=*;
main()==>class@anonymous::__invoke      : ct=       1; wt=*;
main()==>foo                            : ct=       2; wt=*;
main()==>tideways_xhprof_disable        : ct=       1; wt=*;
main()==>{closure}                      : ct=       1; wt=*;
