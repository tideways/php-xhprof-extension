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

qafooprofiler_enable(0, array('argument_functions' => array(
    'Symfony\\Component\\EventDispatcher\\EventDispatcher::dispatch',
    'Zend\\EventManager\\EventManager::trigger',
    'Doctrine\\Common\\EventManager::dispatchEvent',
)));

$dispatcher = new EventDispatcher();
$dispatcher->dispatch('foo', new Event());
$dispatcher->dispatch('bar', new Event());

$manager = new EventManager();
$manager->trigger("baz", new \stdClass, array());

$doctrine = new DoctrineEventManager();
$doctrine->dispatchEvent('event', new Event());

print_canonical(qafooprofiler_disable());
--EXPECT--
main()                                  : ct=       1; wt=*;
main()==>Doctrine\Common\EventManager::dispatchEvent#event: ct=       1; wt=*;
main()==>Symfony\Component\EventDispatcher\EventDispatcher::dispatch#bar: ct=       1; wt=*;
main()==>Symfony\Component\EventDispatcher\EventDispatcher::dispatch#foo: ct=       1; wt=*;
main()==>Zend\EventManager\EventManager::trigger#baz: ct=       1; wt=*;
main()==>qafooprofiler_disable          : ct=       1; wt=*;
