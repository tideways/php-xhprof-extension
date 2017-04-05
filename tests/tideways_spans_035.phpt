--TEST--
Tideways: annotate multipe times regression https://github.com/tideways/php-profiler-extension/issues/37
--FILE--
<?php

require_once __DIR__ . '/common.php';

tideways_enable(TIDEWAYS_FLAGS_NO_HIERACHICAL);

$span = tideways_span_create('php');
tideways_span_annotate($span, ['title' => 'something']);
tideways_span_timer_start($span);

$iterations = 1;
$skipped = 1000;

tideways_span_annotate($span, ['iterations' => $iterations]);
tideways_span_annotate($span, ['skipped' => $skipped]);
tideways_span_annotate($span, ['found normal quest' => 0]);

tideways_span_timer_stop($span);

tideways_disable();

print_spans(tideways_get_spans());
--EXPECTF--
app: 1 timers - cpu=%d
php: 1 timers - found normal quest=0 iterations=1 skipped=1000 title=something
