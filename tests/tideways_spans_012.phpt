--TEST--
Tideways: fastcgi_finish_request() support
--FILE--
<?php

function fastcgi_finish_request() { // only exists on php-fpm
}

tideways_enable();

usleep(100);
fastcgi_finish_request();
usleep(100);

tideways_disable();

var_dump(tideways_get_spans());
--EXPECTF--
array(1) {
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
    array(2) {
      [0]=>
      int(%d)
      [1]=>
      int(%d)
    }
    ["a"]=>
    array(1) {
      ["cpu"]=>
      string(%d) "%d"
    }
  }
}
