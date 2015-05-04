--TEST--
Tideways: curl_exec spans
--SKIPIF--
<?php
if (!extension_loaded('curl')) {
    echo "skip: curl required\n";
    die;
}
--FILE--
<?php

require_once __DIR__ . '/common.php';

tideways_enable();

$ch = curl_init("http://localhost/phpinfo.php");
curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
curl_exec($ch);

$ch = curl_init("http://localhost/phpinfo.php");
curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
curl_exec($ch);

print_spans(tideways_get_spans());
tideways_disable();
--EXPECTF--
app: 1 timers - 
http: 2 timers - title=http://localhost/phpinfo.php
