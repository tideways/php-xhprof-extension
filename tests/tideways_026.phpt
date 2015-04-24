--TEST--
XHProf: eval()
--FILE--
<?php

include_once dirname(__FILE__).'/common.php';

tideways_enable();

eval("strlen('Hello World!');");

$data = tideways_disable();

$spans = tideways_get_spans();
echo $spans[0]['a']['cct'];

--EXPECTF--
1
