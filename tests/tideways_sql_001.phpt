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

    if ($actualSummary === $expectedSummary) {
        echo ++$i . ") OK " . $actualSummary . "\n";
    } else {
        echo ++$i . ") FAIL got '" . $actualSummary . "' but expected '" . $expectedSummary . "'\n";
    }
}
--EXPECTF--
1) OK select foo
2) OK update foo
3) OK insert bar
4) OK delete baz
5) OK commit
6) OK other
