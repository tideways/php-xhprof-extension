--TEST--
Tideways: Span Annotations
--FILE--
<?php

tideways_enable();

$span = tideways_span_create('app');
tideways_span_annotate($span, array('foo' => 'bar', 'bar' => 'baz'));

var_dump(tideways_get_spans());
--EXPECTF--
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
    array(2) {
      ["foo"]=>
      string(3) "bar"
      ["bar"]=>
      string(3) "baz"
    }
  }
}
