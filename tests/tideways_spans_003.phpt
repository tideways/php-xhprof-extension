--TEST--
Tideways: Span Annotations
--FILE--
<?php

tideways_enable();

$span = tideways_span_create('app');
tideways_span_annotate($span, array('foo' => 'bar', 'bar' => 'baz'));
tideways_disable();

tideways_span_annotate($span, array('baz' => 1));

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
      string(%d) "%d"
    }
  }
  [1]=>
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
    array(3) {
      ["foo"]=>
      string(3) "bar"
      ["bar"]=>
      string(3) "baz"
      ["baz"]=>
      string(1) "1"
    }
  }
}
