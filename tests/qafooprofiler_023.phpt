--TEST--
XHProf: Transaction Name Detection in Hierachical Profiling Mode
--FILE--
<?php

use Symfony\Component\HttpKernel\Controller\ControllerResolver;
use Zend\MVC\Controller\ControllerManager;

require "qafooprofiler_023_classes.php";

abstract class Zend_Controller_Action {
    public function dispatch($method)
    {
    }
}
class MyController extends Zend_Controller_Action {
}
class oxView {
    public function setClassName($sClass) {
    }
}
abstract class Enlight_Controller_Action {
    public function dispatch($method)
    {
    }
}
class ShopController extends Enlight_Controller_Action {
}
function get_query_template($name) {
}

function transaction_symfony2() {
    qafooprofiler_enable(0, array(
        'transaction_function' => 'Symfony\Component\HttpKernel\Controller\ControllerResolver::createController',
    ));

    $resolver = new ControllerResolver();
    $resolver->createController("foo::bar");

    echo "Symfony2: " . qafooprofiler_transaction_name() . "\n";
    qafooprofiler_disable();
}

function transaction_zf1() {
    qafooprofiler_enable(0, array(
        'transaction_function' => 'Zend_Controller_Action::dispatch',
    ));
    $controller = new MyController();
    $controller->dispatch("fooAction");

    echo "Zend Framework 1: " . trim(qafooprofiler_transaction_name()) . "\n";
    $data = qafooprofiler_disable();
}

function transaction_zf2() {
    qafooprofiler_enable(0, array(
        'transaction_function' => 'Zend\\MVC\\Controller\\ControllerManager::get',
    ));
    $manager = new ControllerManager();
    $manager->get("FooCtrl", array("foo" => "bar"), false);

    echo "Zend Framework 2: " . qafooprofiler_transaction_name() . "\n";
    $data = qafooprofiler_disable();
}

function transaction_oxid() {
    qafooprofiler_enable(0, array(
        'transaction_function' => 'oxView::setClassName',
    ));

    $shop = new oxView();
    $shop->setClassName("alist");

    echo "Oxid: " . qafooprofiler_transaction_name() . "\n";
    $data = qafooprofiler_disable();
}

function transaction_shopware() {
    qafooprofiler_enable(0, array(
        'transaction_function' => 'Enlight_Controller_Action::dispatch',
    ));

    $controller = new ShopController();
    $controller->dispatch('listAction');

    echo "Shopware: " . trim(qafooprofiler_transaction_name()) . "\n";
    $data = qafooprofiler_disable();
}

function transaction_wordpress() {
    qafooprofiler_enable(0, array(
        'transaction_function' => 'get_query_template',
    ));

    get_query_template("home");

    echo "Wordpress: " . qafooprofiler_transaction_name() . "\n";
    $data = qafooprofiler_disable();
}

transaction_symfony2();
transaction_zf2();
transaction_oxid();
transaction_shopware();
transaction_wordpress();
transaction_zf1();

--EXPECTF--
Symfony2: foo::bar
Zend Framework 2: FooCtrl
Oxid: alist
Shopware: ShopController::listAction
Wordpress: home
Zend Framework 1: MyController::fooAction
