--TEST--
Tideways: Start/Stop Timers
--FILE--
<?php

tideways_enable();

$span1 = tideways_span_create('php');
tideways_span_timer_start($span1);
tideways_span_timer_stop($span1);

$span2 = tideways_span_create('php');
tideways_span_timer_start($span2);
tideways_span_timer_stop($span2);
tideways_span_timer_start($span2);
tideways_span_timer_stop($span2);

tideways_disable();

$spans = tideways_get_spans();
var_dump($spans);

--EXPECTF--
array(3) {
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
  array(3) {
    ["n"]=>
    string(3) "php"
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
  }
  [2]=>
  array(3) {
    ["n"]=>
    string(3) "php"
    ["b"]=>
    array(2) {
      [0]=>
      int(%d)
      [1]=>
      int(%d)
    }
    ["e"]=>
    array(2) {
      [0]=>
      int(%d)
      [1]=>
      int(%d)
    }
  }
}
