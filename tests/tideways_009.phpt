--TEST--
XHProf: Function Argument Profiling
--FILE--
<?php

include_once dirname(__FILE__).'/common.php';

function test_stringlength($string)
{
    return strlen($string);
}

tideways_enable(TIDEWAYS_FLAGS_MEMORY, array('functions' => array('strlen')));
test_stringlength('foo_array');
$output = tideways_disable();

if (count($output) == 2 && isset($output['main()']) && isset($output['main()==>strlen'])) {
    echo "Test passed";
} else {
    var_dump($output);
}
?>
--EXPECT--
Test passed
