--TEST--
Tideways: SoapClient::__doRequest support
--FILE--
<?php

include __DIR__ . '/common.php';

$client = new SoapClient(NULL,array('location'=>'http://localhost', 'uri'=>'http://testuri.org'));

tideways_enable(0, array('ignored_functions' => array('SoapClient::__call')));

try {
    $result = $client->Add(10, 10);
} catch (\SoapFault $f) {
}

$data = tideways_disable();

print_Spans(tideways_get_spans());
--EXPECT--
app: 1 timers - 
http.soap: 1 timers - title=http://localhost
