--TEST--
Tideways: Autodetect file_get_contents() HTTP Spans
--FILE--
<?php

tideways_enable(0, array());

file_get_contents("http://localhost");

var_dump(tideways_get_spans());

tideways_disable();

--EXPECTF--
