--TEST--
Tideways: Elasticsearch PHP Client
--FILE--
<?php

require_once __DIR__ . '/common.php';
require_once __DIR__ . '/elasticsearch.php';

tideways_enable(TIDEWAYS_FLAGS_NO_HIERACHICAL);

$connection = new \Elasticsearch\Connections\Connection();
$connection->performRequest('GET', '/idx1/type2/_search');
$endpoint = new \Elasticsearch\Endpoints\SomeEndpoint();
$endpoint->resultOrFuture(array());

$connection = new \Elasticsearch\Connections\Connection();
$connection->performRequest('GET', '/idx3/type1');
$endpoint = new \Elasticsearch\Endpoints\AnotherEndpoint();
$endpoint->resultOrFuture(array());

$connection = new \Elasticsearch\Connections\GuzzleConnection();
$result = $connection->performRequest('GET', '/idx4/type1');

tideways_disable();
print_spans(tideways_get_spans());
--EXPECTF--
app: 1 timers - cpu=%d
elasticsearch: 1 timers - endpoint=Elasticsearch\Endpoints\SomeEndpoint es.method=GET es.path=/idx1/type2/_search
elasticsearch: 1 timers - endpoint=Elasticsearch\Endpoints\AnotherEndpoint es.method=GET es.path=/idx3/type1
elasticsearch: 1 timers - es.method=GET es.path=/idx4/type1
