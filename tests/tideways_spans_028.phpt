--TEST--
Tideways: MongoDB
--INI--
extension=mongodb.so
--SKIPIF--
<?php
if (!extension_loaded('mongodb')) {
    echo 'skip: required extension "mongodb"';
    exit;
}

$host = isset($_SERVER['MONGO_HOST']) ? $_SERVER['MONGO_HOST'] : '127.0.0.1:27017';

if (!@fopen($host)) {
    echo 'skip: no mongodb is running';
    exit;
}
--FILE--
<?php

require_once __DIR__ . '/common.php';
tideways_enable(TIDEWAYS_FLAGS_NO_HIERACHICAL);

$host = isset($_SERVER['MONGO_HOST']) ? $_SERVER['MONGO_HOST'] : '127.0.0.1:27017';

$bulk = new MongoDB\Driver\BulkWrite();

$bulk->insert(['_id' => 1, 'x' => 1]);
$bulk->insert(['_id' => 2, 'x' => 2]);

$bulk->update(['x' => 2], ['$set' => ['x' => 1]], ['multi' => false, 'upsert' => false]);
$bulk->update(['x' => 3], ['$set' => ['x' => 3]], ['multi' => false, 'upsert' => true]);
$bulk->update(['_id' => 3], ['$set' => ['x' => 3]], ['multi' => false, 'upsert' => true]);

$bulk->insert(['_id' => 4, 'x' => 2]);

$bulk->delete(['x' => 1], ['limit' => 1]);

$manager = new MongoDB\Driver\Manager('mongodb://' . $host);

$command = new MongoDB\Driver\Command(array("drop" => 'collection'));
$manager->executeCommand('db', $command);

$writeConcern = new MongoDB\Driver\WriteConcern(MongoDB\Driver\WriteConcern::MAJORITY, 100);
$result = $manager->executeBulkWrite('db.collection', $bulk, $writeConcern);

$bulk = new MongoDB\Driver\BulkWrite;
for ($i = 100; $i < 1000; $i++) {
    $bulk->insert(['x' => $i]);
}
$manager->executeBulkWrite('db.collection', $bulk);

$filter = ['x' => ['$gt' => 1]];
$options = [
    'projection' => ['_id' => 0],
    'sort' => ['x' => -1],
];

function iterate($cursor) {
    $count = 0;
    foreach ($cursor as $document) {
        $count++;
    }
}

$query = new MongoDB\Driver\Query($filter, $options);
$cursor = $manager->executeQuery('db.collection', $query);

iterate($cursor);

tideways_disable();

print_spans(tideways_get_spans());
--EXPECTF--
app: 1 timers - cpu=%d
mongodb: 1 timers - host=172.17.0.2 op=connect port=27017
mongodb: 1 timers - ns=db op=executeCommand
mongodb: 1 timers - ns=db.collection op=executeBulkWrite
mongodb: 1 timers - ns=db.collection op=executeBulkWrite
mongodb: 1 timers - ns=db.collection op=executeQuery
