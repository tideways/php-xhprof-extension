--TEST--
Tideways: Phalcon MVC Transaction
--FILE--
<?php

namespace Phalcon;

class Dispatcher
{
    private $controller;
    private $action;

    public function __construct($controller, $action)
    {
        $this->controller = $controller;
        $this->action = $action;
    }

    public function getControllerName()
    {
        return $this->controller;
    }

    public function getActionName()
    {
        return $this->action;
    }

    public function getReturnedValue()
    {
    }
}

tideways_enable(0, array('transaction_function' => 'Phalcon\Dispatcher::getReturnedValue'));

$d = new Dispatcher("FooController", "barAction");
$d->getReturnedValue();

echo "name: " . tideways_transaction_name() . "\n";
tideways_disable();

tideways_enable(0, array('transaction_function' => 'Phalcon\Dispatcher::getReturnedValue'));

$d = new Dispatcher(NULL, NULL);
$d->getReturnedValue();

echo "empty: " . tideways_transaction_name() . "\n";
tideways_disable();

tideways_enable(0, array('transaction_function' => 'Phalcon\Dispatcher::getReturnedValue'));

$d = new Dispatcher("ctrl", NULL);
$d->getReturnedValue();

echo "empty2: " . tideways_transaction_name() . "\n";
tideways_disable();

tideways_enable(0, array('transaction_function' => 'Phalcon\Dispatcher::getReturnedValue'));

$d = new Dispatcher(NULL, "action");
$d->getReturnedValue();

echo "empty3: " . tideways_transaction_name() . "\n";
tideways_disable();
--EXPECTF--
name: FooController::barAction
empty: 
empty2: 
empty3: 

