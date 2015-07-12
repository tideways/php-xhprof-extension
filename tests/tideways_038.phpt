--TEST--
Tideways: Test CPU Timer
--FILE--
<?php

tideways_enable(TIDEWAYS_FLAGS_CPU);

sleep(1);

function foo() {
    for ($i = 0; $i < 1000000; $i++) {}
}
foo();

$data = tideways_disable();

if (($data['main()']['wt'] - $data['main()']['cpu']) > 1000000) {
    echo 'OK';
} else {
    echo 'FAIL';
}
--EXPECTF--
OK
