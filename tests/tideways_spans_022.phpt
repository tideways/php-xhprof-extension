--TEST--
Tideways: Limit Size of String Annotation generated from Internal APIs to 1000
--FILE--
<?php

tideways_enable();

@file_get_contents("http://localhost/?" . str_repeat("a", 2000));

tideways_disable();
$spans = tideways_get_spans();
echo strlen($spans[1]['a']['url']);
--EXPECTF--
1000
