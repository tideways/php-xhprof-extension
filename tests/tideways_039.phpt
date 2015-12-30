--TEST--
Tideways: Auto Prepend File 1
--INI--
auto_prepend_file=tests/tideways_039_prepend.php
tideways.auto_prepend_library=0
--FILE--
<?php
--EXPECT--
Hello Auto Prepend!
