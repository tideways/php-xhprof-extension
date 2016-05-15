--TEST--
Tideways: CakePHP 3 Transaction Detection
--FILE--
<?php

namespace Cake\Controller;

require_once __DIR__ . '/common.php';

// Its actually at Cake\Network\Request, but thats not important for the test
class Request {
    public $params;
}

class Controller {
    public $request;
    public function invokeAction() {
    }
}

class UserController extends Controller {
}

tideways_enable(0, array(
    'transaction_function' => 'Cake\\Controller\\Controller::invokeAction'
));

$request = new Request();
$ctrl = new UserController();
$ctrl->invokeAction();
@$ctrl::invokeAction();

$request->params['action'] = 'foo';

$ctrl->request = $request;
$ctrl->invokeAction();

echo tideways_transaction_name() . "\n";
print_spans(tideways_get_spans());
--EXPECTF--
Cake\Controller\UserController::foo
app: 1 timers - 
php.ctrl: 1 timers - title=Cake\Controller\UserController::foo
