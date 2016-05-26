--TEST--
Tideways: yii2 Transaction Detection
--FILE--
<?php

namespace yii\base;

abstract class Module
{
    public function runAction($actionId, $params = array())
    {
    }
}

class UserController extends Module
{
}

tideways_enable(0, array('transaction_function' => 'yii\base\Module::runAction'));

$ctrl = new UserController();
$ctrl->runAction('view');

echo 'view: ' . tideways_transaction_name() . "\n";

tideways_disable();

tideways_enable(0, array('transaction_function' => 'yii\base\Module::runAction'));
$ctrl = new UserController();
$ctrl->runAction(NULL);
echo 'empty: ' . tideways_transaction_name() . "\n";
--EXPECTF--
view: yii\base\UserController::view
empty: 

