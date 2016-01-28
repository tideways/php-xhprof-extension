--TEST--
Tideways: Event Spans
--FILE--
<?php

use Symfony\Component\EventDispatcher\Event;
use Symfony\Component\EventDispatcher\EventDispatcher;
use Zend\EventManager\EventManager;
use Doctrine\Common\EventManager as DoctrineEventManager;

include_once dirname(__FILE__).'/common.php';
include_once dirname(__FILE__).'/tideways_events1.php';
include_once dirname(__FILE__).'/tideways_events2.php';

tideways_enable();

$dispatcher = new EventDispatcher();
$dispatcher->dispatch('foo', new Event());
$dispatcher->dispatch('bar', new Event());

$manager = new EventManager();
$manager->trigger("baz", new \stdClass, array());

$doctrine = new DoctrineEventManager();
$doctrine->dispatchEvent('event', new Event());

$enlight = new Enlight_Event_EventManager();
$enlight->filter("foo", 1234, new Event());
$enlight->notify("bar", new Event());
$enlight->notifyUntil("baz", new Event());

Mage::dispatchEvent('zoomzoom', array());

do_action("foo", array("foo" => "bar"));
drupal_alter("foo", 1, 2, 3, 4);

$flow3 = new \TYPO3\Flow\SignalSlot\Dispatcher();
$flow3->dispatch('signal', 'slot');

$spans = tideways_get_spans();
print_spans($spans);
tideways_disable();
--EXPECTF--
app: 1 timers - 
event: 4 timers - title=foo
event: 2 timers - title=bar
event: 2 timers - title=baz
event: 1 timers - title=event
event: 1 timers - title=zoomzoom
event: 1 timers - title=signal::slot
