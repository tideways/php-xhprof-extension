--TEST--
Tideways: curl_exec spans
--SKIPIF--
<?php
if (!extension_loaded('curl')) {
    echo "skip: curl required\n";
    die;
}
if (PHP_VERSION_ID < 54000) {
    echo "skip: PHP 5.4+ only test\n";
    exit();
}
--FILE--
<?php

require_once __DIR__ . '/common.php';
http_cli_server_start();

tideways_enable();

$ch = curl_init(PHP_HTTP_SERVER_ADDRESS);
curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
curl_exec($ch);

$ch = curl_init(PHP_HTTP_SERVER_ADDRESS . "?foo=bar");
curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
curl_exec($ch);

print_spans(tideways_get_spans());
tideways_disable();
--EXPECTF--
app: 1 timers - 
http: 1 timers - url=%s
http: 1 timers - url=%s
