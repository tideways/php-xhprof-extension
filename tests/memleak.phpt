--TEST--
--FILE--
<?php

tideways_enable(TIDEWAYS_FLAGS_NO_HIERACHICAL);
$data = tideways_disable();
--EXPECTF--
