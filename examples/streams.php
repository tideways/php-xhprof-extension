<?php

$options = array('argument_functions' => array('fopen', 'fgets', 'fclose'));

xhprof_enable(0, $options);

$fh = fopen(__DIR__, "r");
fgets($fh);
fclose($fh);

$fh = fopen("http://qafoo.com/impressum.html?foo=bar", "r");
fgets($fh);
fclose($fh);

$fh = fopen("http://php.net", "r");
fgets($fh);
fclose($fh);

var_dump(xhprof_disable());
