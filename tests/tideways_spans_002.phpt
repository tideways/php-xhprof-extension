--TEST--
Tideways: Start/Stop Timers
--FILE--
<?php

tideways_enable();

$span1 = tideways_create_span('php');
tideways_timer_start($span1);
tideways_timer_stop($span1);

$span2 = tideways_create_span('php');
tideways_timer_start($span2);
tideways_timer_stop($span2);
tideways_timer_start($span2);
tideways_timer_stop($span2);

$spans = tideways_get_spans();
var_dump($spans);
--EXPECTF--
array(2) {
  [0]=>
  array(4) {
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
    ["a"]=>
    array(0) {
    }
  }
  [1]=>
  array(4) {
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
    ["a"]=>
    array(0) {
    }
  }
}
