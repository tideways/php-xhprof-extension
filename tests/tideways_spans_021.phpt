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

--EXPECTF--
1500
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
  array(1) {
    ["foo"]=>
    string(4) "1497"
  }
}
