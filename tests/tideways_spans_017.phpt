--TEST--
Tideways: Watch with Callback
--FILE--
<?php

function foo_cb($context) {
    var_dump("inside watcher");
    var_dump($context);
    return tideways_span_create('doctrine');
}

function no_span_cb($context) {
}

function foo($x, $y) {
}

function no_span() {
}

function throwex_cb() {
    throw new \InvalidArgumentException();
}
function throwex() {}

function closured($x, $y) {}

class Foo {
    private $value = 'hi!';
    public function bar() {}
}

tideways_enable();
tideways_span_callback('foo', 'foo_cb');
tideways_span_callback('no_span', 'no_span_cb');
tideways_span_callback('throwex', 'throwex_cb');
tideways_span_callback('closured', function ($context) {
    echo "Inside Closure\n";
    var_dump($context);
});
tideways_span_callback('Foo::bar', function ($context) {
    echo "With Object\n";
    var_dump($context);

    $GLOBALS['context'] = $context;
});

foo(1, 2);
no_span();
try {
    throwex_cb();
} catch (\InvalidArgumentException $e) {
}
closured("foo", "bar");

$foo = new Foo();
$foo->bar("baz");

echo "Context Dumped From Global State\n";
var_dump($GLOBALS['context']);

tideways_disable();
echo "\nSpans\n";
var_dump(tideways_get_spans());

--EXPECTF--
string(14) "inside watcher"
array(2) {
  ["fn"]=>
  string(3) "foo"
  ["args"]=>
  array(2) {
    [0]=>
    int(1)
    [1]=>
    int(2)
  }
}
Inside Closure
array(2) {
  ["fn"]=>
  string(8) "closured"
  ["args"]=>
  array(2) {
    [0]=>
    string(3) "foo"
    [1]=>
    string(3) "bar"
  }
}
With Object
array(3) {
  ["fn"]=>
  string(8) "Foo::bar"
  ["args"]=>
  array(1) {
    [0]=>
    string(3) "baz"
  }
  ["object"]=>
  object(Foo)#4 (1) {
    ["value":"Foo":private]=>
    string(3) "hi!"
  }
}
Context Dumped From Global State
array(3) {
  ["fn"]=>
  string(8) "Foo::bar"
  ["args"]=>
  array(1) {
    [0]=>
    string(3) "baz"
  }
  ["object"]=>
  object(Foo)#4 (1) {
    ["value":"Foo":private]=>
    string(3) "hi!"
  }
}

Spans
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
  array(3) {
    ["n"]=>
    string(8) "doctrine"
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
}
