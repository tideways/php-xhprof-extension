<?php

function double($x) {
    return $x * 2;
}

$doubles = array_map('double', range(1, 10));
