--TEST--
Tideways: Doctrine Span Support
--FILE--
<?php

include __DIR__ . "/common.php";
include __DIR__ . "/tideways_doctrine.php";

tideways_enable();

$persister = new \Doctrine\ORM\Persisters\BasicEntityPersister("Foo");
$persister->load();
$persister->load();

$persister = new \Doctrine\ORM\Persisters\BasicEntityPersister("Bar");
$persister->load();

$query = new \Doctrine\ORM\Query();
$query->execute();
$query->execute();

$query->getDQL();

$query = new \Doctrine\ORM\NativeQuery();
$query->execute();

$query->getSQL();

print_spans(tideways_get_spans());
tideways_disable();
--EXPECT--
doctrine.load: 2 timers - title=Foo
doctrine.load: 1 timers - title=Bar
doctrine.query: 2 timers - title=select Foo\Bar\Baz
doctrine.query: 1 timers - title=select bar
