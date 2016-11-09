--TEST--
Tideways: Cross Limit of 1500 spans
--FILE--
<?php

function testing(array $annotations) {
}

tideways_enable();
tideways_span_callback('testing', function ($context) {
    $id = tideways_span_create('php');
    tideways_span_annotate($id, $context['args'][0]);
    return $id;
});

$id = tideways_span_create("test");
tideways_span_annotate($id, array("test" => "before"));

for ($i = 0; $i < 2000; $i++) {
    testing(array("foo" => "$i"));
}

$id = tideways_span_create("test");
tideways_span_annotate($id, array("test" => "before"));

$spans = tideways_get_spans();
echo count($spans) . "\n";
var_dump($spans[1]);
var_dump($spans[1499]);
var_dump(count($spans[1500]['b']));
var_dump($spans[1500]['a']);
var_dump($spans[1501]);

--EXPECTF--
1502
array(4) {
  ["n"]=>
  string(4) "test"
  ["b"]=>
  array(0) {
  }
  ["e"]=>
  array(0) {
  }
  ["a"]=>
  array(1) {
    ["test"]=>
    string(6) "before"
  }
}
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
  array(2) {
    ["foo"]=>
    string(4) "1497"
    ["fn"]=>
    string(7) "testing"
  }
}
int(502)
array(3) {
  ["foo"]=>
  string(4) "1999"
  ["fn"]=>
  string(7) "testing"
  ["trunc"]=>
  string(1) "1"
}
array(4) {
  ["n"]=>
  string(4) "test"
  ["b"]=>
  array(0) {
  }
  ["e"]=>
  array(0) {
  }
  ["a"]=>
  array(1) {
    ["test"]=>
    string(6) "before"
  }
}
