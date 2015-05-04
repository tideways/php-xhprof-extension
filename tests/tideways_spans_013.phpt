--TEST--
Tideways: SoapClient::__doRequest support
--SKIPIF--
<?php
if (!extension_loaded('soap')) {
    echo "skip: soapclient needs to be installed.\n";
}
if (PHP_VERSION_ID < 50500) {
    echo "skip: only works with PHP 5.5+\n";
}
--FILE--
<?php

include __DIR__ . '/common.php';

$client = new SoapClient(
    'http://ec.europa.eu/taxation_customs/vies/checkVatService.wsdl',
    array('cache_wsdl' => WSDL_CACHE_NONE)
);

tideways_enable(0, array('ignored_functions' => array('SoapClient::__call')));

try {
    $result = $client->checkVat(array(
        'countryCode' => 'DE',
        'vatNumber' => '272316452',
    ));
} catch (\SoapFault $f) {
}

$data = tideways_disable();

print_Spans(tideways_get_spans());
--EXPECT--
app: 1 timers - 
http.soap: 1 timers - title=http://ec.europa.eu/taxation_customs/vies/services/checkVatService
