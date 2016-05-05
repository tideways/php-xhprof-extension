--TEST--
Tidweays: Presta Controller spans
--FILE--
<?php

abstract class ControllerCore {
    public function run() {}
}
class AuthController extends ControllerCore {
}

tideways_enable();
$ctrl = new AuthController();
$ctrl->run();

var_dump(tideways_get_spans());
--EXPECTF--
array(2) {
  [0]=>
  array(3) {
    ["n"]=>
    string(3) "app"
    ["b"]=>
    array(1) {
      [0]=>
      int(%d)
    }
    ["e"]=>
    array(0) {
    }
  }
  [1]=>
  array(4) {
    ["n"]=>
    string(8) "php.ctrl"
    ["b"]=>
    array(1) {
      [0]=>
      int(%d)
    }
    ["e"]=>
    array(1) {
      [0]=>
      int(%d)
    }
    ["a"]=>
    array(2) {
      ["title"]=>
      string(14) "AuthController"
      ["fn"]=>
      string(19) "ControllerCore::run"
    }
  }
}
