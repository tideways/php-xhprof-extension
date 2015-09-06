--TEST--
Tideways: MongoDB Support
--INI--
extension=mongo.so
--SKIPIF--
<?php
if (!extension_loaded('mongo')) {
    die('skip: mongo extension required');
}
if (!file_exists(__DIR__ . '/../modules/mongo.so')) {
    die('skip: must symlink mongo.so into modules directory');
}
try {
    $mongo = new MongoClient();
    $tideways = $mongo->tidewaystest;
    $mongo->dropDB('tidewaystest');
} catch (\Exception $e) {
    die('skip: could not connect to mongo and create tidewaystest db');
}
--FILE--
<?php

require __DIR__ . '/common.php';

tideways_enable();

$mongo = new MongoClient();
$tideways = $mongo->tidewaystest;

$tideways->items->find();
$tideways->items->findOne();
$tideways->items->save(array('x' => 1));

tideways_disable();
print_spans(tideways_get_spans());

--EXPECTF--
app: 1 timers - cpu=%d
mongo: 1 timers - collection=items title=MongoCollection::find
mongo: 1 timers - collection=items title=MongoCollection::findOne
mongo: 1 timers - collection=items title=MongoCollection::save
