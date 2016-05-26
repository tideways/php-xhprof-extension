--TEST--
Tideways: curl_multi_exec / Async Handling
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

tideways_enable(TIDEWAYS_FLAGS_NO_HIERACHICAL);
http_cli_server_start();

$ch1 = curl_init();
$ch2 = curl_init();

curl_setopt($ch1, CURLOPT_URL, PHP_HTTP_SERVER_ADDRESS);
curl_setopt($ch1, CURLOPT_HEADER, 0);
curl_setopt($ch1, CURLOPT_RETURNTRANSFER, true);
curl_setopt($ch2, CURLOPT_URL, PHP_HTTP_SERVER_ADDRESS);
curl_setopt($ch2, CURLOPT_HEADER, 0);
curl_setopt($ch2, CURLOPT_RETURNTRANSFER, true);

$mh = curl_multi_init();

curl_multi_add_handle($mh,$ch1);
curl_multi_add_handle($mh,$ch2);

do {
    curl_multi_exec($mh, $running);
    curl_multi_select($mh);
} while ($running > 0);

while ($done = curl_multi_info_read($mh)) {
    $id = (int) $done['handle'];
    curl_multi_remove_handle($mh, $done['handle']);
}

curl_multi_close($mh);

tideways_disable();
$spans = tideways_get_spans();
print_spans($spans);
--EXPECTF--
app: 1 timers - cpu=%d
http: 1 timers - http.status_code=%d net.in=%d net.out=%d peer.ipv4=%s peer.port=%d url=%s
http: 1 timers - http.status_code=%d net.in=%d net.out=%d peer.ipv4=%s peer.port=%d url=%s
