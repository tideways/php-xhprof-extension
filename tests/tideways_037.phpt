--TEST--
Tideways: Enable stops and discards running trace
--FILE--
<?php

require_once dirname(__FILE__) . "/common.php";

tideways_enable();

strlen("foo");

tideways_enable();

substr("foo", 0, 3);

$output = tideways_disable();
print_canonical($output);
echo "\n";
--EXPECT--
main()                                  : ct=       1; wt=*;
substr                                  : ct=       1; wt=*;
tideways_disable                        : ct=       1; wt=*;
