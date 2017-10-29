--TEST--
Tideways: Keep Stack on long running spans
--INI--
tideways.stack_threshold=0
--SKIPIF--
<?php
if (PHP_VERSION_ID < 50400) {
    echo "skip: php 5.4+ only\n";
}
--FILE--
<?php

require_once __DIR__ . '/common.php';

function myapp() {
    ctrl();
}

function ctrl() {
    foo();
}

function foo() {
    bar();
}

function bar() {
}

tideways_enable(TIDEWAYS_FLAGS_NO_HIERACHICAL);

tideways_span_watch("foo");
tideways_span_watch("bar");

myapp();

tideways_disable();

$spans = (tideways_get_spans());

foreach ($spans as $span) {
    echo $span['n'] . "\n";

    if (isset($span['stack'])) {
        if (PHP_VERSION_ID < 70000) {
            // pop element from beginning to adjust for incompatibilities
            // between PHP 5 and 7
            array_shift($span['stack']); 
        }
        foreach ($span['stack'] as $stack) {
            echo '  ' . $stack['function'] . "\n";
        }
    }
}
--EXPECTF--
app
php
  ctrl
  myapp
php
  foo
  ctrl
  myapp
