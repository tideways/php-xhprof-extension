--TEST--
Tideways: Laravel Eloquent ORM Support
--FILE--
<?php

namespace Illuminate\Database\Eloquent;

require_once __DIR__ . '/common.php';

abstract class Model {
    public function performInsert() {
    }

    public function performUpdate() {
    }

    public function delete() {
    }
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
$builder = new Builder($user = new User());
$builder->getModels();
$user->performUpdate();
$user->performInsert();
$user->delete();
$builder = new Builder("not an object");
$builder->getModels();
tideways_disable();
print_spans(tideways_get_spans());

--EXPECTF--
app: 1 timers - cpu=%d
eloquent: 1 timers - model=Illuminate\Database\Eloquent\User op=get
eloquent: 1 timers - model=Illuminate\Database\Eloquent\User op=performUpdate
eloquent: 1 timers - model=Illuminate\Database\Eloquent\User op=performInsert
eloquent: 1 timers - model=Illuminate\Database\Eloquent\User op=delete
