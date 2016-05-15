--TEST--
Tideways: CakePHP 2 Transaction Detection
--FILE--
<?php

require_once __DIR__ . '/common.php';

class CakeRequest {
    public $params;
}

class Controller
{
    public function invokeAction(CakeRequest $request = null) {
    }
}

class UserController extends Controller {
}

tideways_enable(0, array('transaction_function' => 'Controller::invokeAction'));

$request = new CakeRequest();
$ctrl = new UserController();
$ctrl->invokeAction(null);
@$ctrl::invokeAction($request);
$ctrl->invokeAction($request);

$request->params['action'] = 'foo';

$ctrl->invokeAction($request);

echo tideways_transaction_name() . "\n";
print_spans(tideways_get_spans());
--EXPECTF--
UserController::foo
app: 1 timers - 
php.ctrl: 1 timers - title=UserController::foo
