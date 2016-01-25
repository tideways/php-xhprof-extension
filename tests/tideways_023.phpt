--TEST--
XHProf: Transaction Name Detection in Hierachical Profiling Mode
--FILE--
<?php

use Symfony\Component\HttpKernel\Controller\ControllerResolver;
use Zend\MVC\Controller\ControllerManager;

require "tideways_023_classes.php";

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
    tideways_enable(TIDEWAYS_FLAGS_NO_SPANS, array(
        'transaction_function' => 'Symfony\Component\HttpKernel\Controller\ControllerResolver::createController',
    ));

    $resolver = new ControllerResolver();
    $resolver->createController("foo::bar");

    echo "Symfony2: " . tideways_transaction_name() . "\n";
    tideways_disable();
}

function transaction_zf1() {
    tideways_enable(TIDEWAYS_FLAGS_NO_SPANS, array(
        'transaction_function' => 'Zend_Controller_Action::dispatch',
    ));
    $controller = new MyController();
    $controller->dispatch("fooAction");

    echo "Zend Framework 1: " . trim(tideways_transaction_name()) . "\n";
    $data = tideways_disable();
}

function transaction_zf2() {
    tideways_enable(TIDEWAYS_FLAGS_NO_SPANS, array(
        'transaction_function' => 'Zend\\MVC\\Controller\\ControllerManager::get',
    ));
    $manager = new ControllerManager();
    $manager->get("FooCtrl", array("foo" => "bar"), false);

    echo "Zend Framework 2: " . tideways_transaction_name() . "\n";
    $data = tideways_disable();
}

function transaction_oxid() {
    tideways_enable(TIDEWAYS_FLAGS_NO_SPANS, array(
        'transaction_function' => 'oxView::setClassName',
    ));

    $shop = new oxView();
    $shop->setClassName("alist");

    echo "Oxid: " . tideways_transaction_name() . "\n";
    $data = tideways_disable();
}

function transaction_shopware() {
    tideways_enable(TIDEWAYS_FLAGS_NO_SPANS, array(
        'transaction_function' => 'Enlight_Controller_Action::dispatch',
    ));

    $controller = new ShopController();
    $controller->dispatch('listAction');

    echo "Shopware: " . trim(tideways_transaction_name()) . "\n";
    $data = tideways_disable();
}

function transaction_wordpress() {
    tideways_enable(TIDEWAYS_FLAGS_NO_SPANS, array(
        'transaction_function' => 'get_query_template',
    ));

    get_query_template("home");

    echo "Wordpress: " . tideways_transaction_name() . "\n";
    $data = tideways_disable();
}

function transaction_laravel() {
    tideways_enable(TIDEWAYS_FLAGS_NO_SPANS, array(
        'transaction_function' => 'Illuminate\Routing\Controller::callAction',
    ));

    $ctrl = new \CachetHQ\Cachet\Http\Controllers\RssController();
    $ctrl->callAction('indexAction', array());

    echo "Laravel: " . tideways_transaction_name() . "\n";
    $data = tideways_disable();
}

function transaction_flow3() {
    tideways_enable(TIDEWAYS_FLAGS_NO_SPANS, array(
        'transaction_function' => 'TYPO3\Flow\Mvc\Controller\ActionController::callActionMethod',
    ));

    $ctrl = new \TYPO3\Flow\Mvc\Controller\FooController();
    $ctrl->processRequest();

    echo "FLOW3: " . tideways_transaction_name() . "\n";
    $data = tideways_disable();
}

transaction_symfony2();
transaction_zf2();
transaction_oxid();
transaction_shopware();
transaction_wordpress();
transaction_zf1();
transaction_laravel();
transaction_flow3();

--EXPECTF--
Symfony2: foo::bar
Zend Framework 2: FooCtrl
Oxid: alist
Shopware: ShopController::listAction
Wordpress: home
Zend Framework 1: MyController::fooAction
Laravel: CachetHQ\Cachet\Http\Controllers\RssController::indexAction
FLOW3: TYPO3\Flow\Mvc\Controller\FooController::indexAction
