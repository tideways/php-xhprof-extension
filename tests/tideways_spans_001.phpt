--TEST--
Tideways: Span Create/Get
--FILE--
<?php

$span = tideways_create_span('app');
$spans = tideways_get_spans();

var_dump($span);
var_dump($spans);

tideways_enable();

$span = tideways_create_span('php');
$spans = tideways_get_spans();

var_dump($span);
var_dump($spans);

tideways_disable();

tideways_enable();

$span = tideways_create_span('app');
$spans = tideways_get_spans();

var_dump($span);
var_dump($spans);

tideways_disable();

--EXPECTF--
NULL
NULL
int(0)
array(1) {
  [0]=>
  array(4) {
    ["n"]=>
    string(3) "php"
    ["b"]=>
    array(0) {
    }
    ["e"]=>
    array(0) {
    }
    ["a"]=>
    array(0) {
    }
  }
}
int(0)
array(1) {
  [0]=>
  array(4) {
    ["n"]=>
    string(3) "app"
    ["b"]=>
    array(0) {
    }
    ["e"]=>
    array(0) {
    }
    ["a"]=>
    array(0) {
    }
  }
}
