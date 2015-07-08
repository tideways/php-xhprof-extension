--TEST--
Tideways: Zend Framework 1
--FILE--
<?php

include __DIR__ . '/common.php';

class Zend_View_Abstract
{
    public function render($name) {}
}

tideways_enable();

$view = new Zend_View_Abstract();
$view->render('foo/bar.phtml');

print_spans(tideways_get_spans());
--EXPECT--
app: 1 timers - 
view: 1 timers - title=foo/bar.phtml
