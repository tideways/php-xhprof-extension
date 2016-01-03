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

$rsm = new \Doctrine\ORM\Query\ResultSetMapping();
$rsm->aliasMap["f"] = "Foo\Bar\Baz";
$query = new \Doctrine\ORM\Query();
$query->setResultSetMapping($rsm);
$query->execute();
$query->execute();

$query->getDQL();

$query = new \Doctrine\ORM\NativeQuery();
$query->setResultSetMapping($rsm);
$query->execute();

$query->getSQL();

print_spans(tideways_get_spans());
tideways_disable();
--EXPECT--
app: 1 timers - 
doctrine.load: 2 timers - title=Foo
doctrine.load: 1 timers - title=Bar
doctrine.query: 1 timers - sql=SELECT f FROM Foo\Bar\Baz title=DQL
doctrine.query: 1 timers - sql=SELECT f FROM Foo\Bar\Baz title=DQL
doctrine.query: 1 timers - title=Native
