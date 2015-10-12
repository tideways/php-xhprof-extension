--TEST--
Tideways: Queue Support
--FILE--
<?php

require_once __DIR__ . '/common.php';
require_once __DIR__ . '/tideways_queue.php';

tideways_enable();

$queue = new \Pheanstalk\Pheanstalk;
$queue->put();
$queue->putInTube("foo");
$queue->putInTube("bar");

$queue = new \PhpAmqpLib\Channel\AMQPChannel;
$queue->basic_publish(array(), 'amqp.foo');
$queue->basic_publish(array(), 'amqp.bar');

tideways_disable();
print_spans(tideways_get_spans());
--EXPECTF--
app: 1 timers - cpu=%d
queue: 1 timers - title=default
queue: 1 timers - title=foo
queue: 1 timers - title=bar
queue: 1 timers - title=amqp.foo
queue: 1 timers - title=amqp.bar
