--TEST--
Tideways: Check for no memleaks
--FILE--
<?php

tideways_enable();
$spans = tideways_get_spans();
$data = tideways_disable();
$spans = tideways_get_spans();
--EXPECTF--
