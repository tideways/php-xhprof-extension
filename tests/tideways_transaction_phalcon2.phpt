--TEST--
Tideways: Phalcon Micro Transaction Detection
--FILE--
<?php

namespace Phalcon\Mvc;

class Route
{
    private $pattern;

    public function __construct($pattern)
    {
        $this->pattern = $pattern;
    }

    public function getPattern()
    {
        return $this->pattern;
    }
}

class Router
{
    private $_matchedRoute;

    public function __construct(Route $route = null)
    {
        $this->_matchedRoute = $route;
    }

    public function getMatchedRoute()
    {
        return $this->_matchedRoute;
    }
}

tideways_enable(0, array('transaction_function' => 'Phalcon\Mvc\Router::getMatchedRoute'));
$router = new Router(new Route("/foo/{id}"));
$router->getMatchedRoute();

echo "#1: " . tideways_transaction_name() . "\n";
tideways_disable();

tideways_enable(0, array('transaction_function' => 'Phalcon\Mvc\Router::getMatchedRoute'));
$router = new Router(NULL);
$router->getMatchedRoute();

echo "#2: " . tideways_transaction_name() . "\n";
tideways_disable();

tideways_enable(0, array('transaction_function' => 'Phalcon\Mvc\Router::getMatchedRoute'));
$router = new Router(new Route(NULL));
$router->getMatchedRoute();

echo "#3: " . tideways_transaction_name() . "\n";
tideways_disable();
--EXPECTF--
#1: /foo/{id}
#2: 
#3: 

