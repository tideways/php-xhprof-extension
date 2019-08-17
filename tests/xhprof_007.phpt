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
call_user_func==>foo                    : ct=       1; wt=*;
call_user_func_array==>class@anonymous::__invoke: ct=       1; wt=*;
call_user_func_array==>foo              : ct=       1; wt=*;
call_user_func_array==>{closure}        : ct=       1; wt=*;
main()                                  : ct=       1; wt=*;
main()==>call_user_func                 : ct=       1; wt=*;
main()==>call_user_func_array           : ct=       3; wt=*;
main()==>tideways_xhprof_disable        : ct=       1; wt=*;
