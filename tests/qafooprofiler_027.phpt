--TEST--
XHPRof: Test EventDispatcher Argument Functions
Author: beberlei
--FILE--
<?php

use Symfony\Component\EventDispatcher\Event;
use Symfony\Component\EventDispatcher\EventDispatcher;
use Zend\EventManager\EventManager;
use Doctrine\Common\EventManager as DoctrineEventManager;

include_once dirname(__FILE__).'/common.php';
include_once dirname(__FILE__).'/qafooprofiler_027_classes.php';

class Mage
{
    static public function dispatchEvent($eventName, $params)
    {
    }
}

class Enlight_Event_EventManager
{
    public function filter($event, $value, $args) {}
    public function notify($event, $args) {}
    public function notifyUntil($event, $args) {}
}

function do_action($name, $params) {
}
function apply_filters($name, $params) {
}
function drupal_alter($name, $args) {
}

qafooprofiler_enable(0, array('argument_functions' => array(
    'Symfony\\Component\\EventDispatcher\\EventDispatcher::dispatch',
    'Zend\\EventManager\\EventManager::trigger',
    'Doctrine\\Common\\EventManager::dispatchEvent',
    'Enlight_Event_EventManager::filter',
    'Enlight_Event_EventManager::notify',
    'Enlight_Event_EventManager::notifyUntil',
    'Mage::dispatchEvent',
    'do_action',
    'apply_filters',
    'drupal_alter',
)));

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
apply_filters("foo", array("foo" => "bar"));
drupal_alter("foo", 1, 2, 3, 4);

print_canonical(qafooprofiler_disable());
--EXPECT--
main()                                  : ct=       1; wt=*;
main()==>Doctrine\Common\EventManager::dispatchEvent#event: ct=       1; wt=*;
main()==>Enlight_Event_EventManager::filter#foo: ct=       1; wt=*;
main()==>Enlight_Event_EventManager::notify#bar: ct=       1; wt=*;
main()==>Enlight_Event_EventManager::notifyUntil#baz: ct=       1; wt=*;
main()==>Mage::dispatchEvent#zoomzoom   : ct=       1; wt=*;
main()==>Symfony\Component\EventDispatcher\EventDispatcher::dispatch#bar: ct=       1; wt=*;
main()==>Symfony\Component\EventDispatcher\EventDispatcher::dispatch#foo: ct=       1; wt=*;
main()==>Zend\EventManager\EventManager::trigger#baz: ct=       1; wt=*;
main()==>apply_filters#foo              : ct=       1; wt=*;
main()==>do_action#foo                  : ct=       1; wt=*;
main()==>drupal_alter#foo               : ct=       1; wt=*;
main()==>qafooprofiler_disable          : ct=       1; wt=*;
