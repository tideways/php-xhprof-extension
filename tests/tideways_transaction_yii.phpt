--TEST--
Tideways: yii Framework Detection
--FILE--
<?php

abstract class CController
{
    public function run($actionID)
    {
    }
}

class UserController extends CController
{
}

tideways_enable(0, array('transaction_function' => 'CController::run'));

$ctrl = new UserController();
$ctrl->run('view');

echo 'view: ' . tideways_transaction_name() . "\n";

tideways_disable();

tideways_enable(0, array('transaction_function' => 'CController::run'));
$ctrl = new UserController();
$ctrl->run(NULL);
echo 'empty: ' . tideways_transaction_name() . "\n";
--EXPECTF--
view: UserController::view
empty: 

