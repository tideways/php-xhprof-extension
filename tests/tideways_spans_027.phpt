--TEST--
Tideways: Laravel Eloquent ORM Support
--FILE--
<?php

namespace Illuminate\Database\Eloquent;

abstract class Model {
}

class Builder {
    protected $model;

    public function __construct($model) {
        $this->model = $model;
    }

    public function getModel() {
        return $this->model;
    }

    public function getModels() {
    }
}

class User extends Model {
}

tideways_enable();
(new Builder(new User()))->getModels();
(new Builder("not an object"))->getModels();
tideways_disable();
var_dump(tideways_get_spans());

--EXPECTF--
array(2) {
  [0]=>
  array(4) {
    ["n"]=>
    string(3) "app"
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
    array(1) {
      ["cpu"]=>
      string(2) "%d"
    }
  }
  [1]=>
  array(4) {
    ["n"]=>
    string(8) "eloquent"
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
      string(33) "Illuminate\Database\Eloquent\User"
      ["fn"]=>
      string(47) "Illuminate\Database\Eloquent\Builder::getModels"
    }
  }
}
