--TEST--
Tideways: Doctrine CouchDB Support
--FILE--
<?php

require_once __DIR__ . '/common.php';
require_once __DIR__ . '/tideways_doctrine.php';

tideways_enable();

$client = new \Doctrine\CouchDB\HTTP\StreamClient();
$client->request('GET', '/_foo');

$client = new \Doctrine\CouchDB\HTTP\SocketClient();
$client->request('GET', '/_bar');

print_spans(tideways_get_spans());
--EXPECTF--
string(9) "GET /_foo"
string(9) "GET /_bar"
app: 1 timers - 
http.couchdb: 1 timers - title=GET /_foo
http.couchdb: 1 timers - title=GET /_bar
