--TEST--
Tideways: SQL Minifier
--FILE--
<?php

$queries = array(
    'SELECT * FROM foo' => 'select foo',
    'UPDATE foo SET bar=baz' => 'update foo',
    'INSERT INTO bar (..) VALUES (...)' => 'insert bar',
    'DELETE FROM baz WHERE bar = 1' => 'delete baz',
    'COMMIT' => 'commit',
    'DROP TABLE' => 'other',
);

$i = 0;
foreach ($queries as $sql => $expectedSummary) {
    $actualSummary = tideways_sql_minify($sql);

    if ($actualSummary === '') {
        echo ++$i . ") OK\n";
    } else {
        echo ++$i . ") FAIL got '" . $actualSummary . "' but expected empty string.\n";
    }
}
--EXPECTF--
1) OK
2) OK
3) OK
4) OK
5) OK
6) OK
