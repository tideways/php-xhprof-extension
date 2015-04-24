--TEST--
Tideways: Symfony Support
--FILE--
<?php

include __DIR__ . '/common.php';
include __DIR__ . '/tideways_symfony.php';

tideways_enable();

$kernel = new \Symfony\Component\HttpKernel\Kernel();
$kernel->boot();

$resolver = new \Symfony\Component\HttpKernel\Controller\ControllerResolver();
$httpKernel = new \Symfony\Component\HttpKernel\HttpKernel($resolver);
$httpKernel->handle();

$resolver = new \Symfony\Component\HttpKernel\Controller\TraceableControllerResolver($resolver);
$httpKernel = new \Symfony\Component\HttpKernel\HttpKernel($resolver);
$httpKernel->handle();

print_spans(tideways_get_spans());
tideways_disable();
--EXPECTF--
app: 1 timers - 
php: 1 timers - title=Symfony\Component\HttpKernel\Kernel::boot
php: 1 timers - title=Acme\DemoBundle\Controller\DefaultController::indexAction
php: 1 timers - title=Acme\DemoBundle\Controller\DefaultController::indexAction
