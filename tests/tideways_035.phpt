--TEST--
Tideways: Function that checks if prepend was overwritten
--FILE--
<?php

$exists = file_exists(ini_get("extension_dir") . "/Tideways.php");

if (tideways_prepend_overwritten() === $exists) {
    echo "OK!";
}
--EXPECTF--
OK!
