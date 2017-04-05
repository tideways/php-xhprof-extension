--TEST--
Tideways: annotate multipe times regression https://github.com/tideways/php-profiler-extension/issues/37
--FILE--
<?php

require_once __DIR__ . '/common.php';

class TwExtensionSpan
{
    /**
     * @var int
     */
    private $idx;

    public static function createSpan($name = null)
    {
        return new self(tideways_span_create($name));
    }

    public function getSpans()
    {
        return tideways_get_spans();
    }

    public function __construct($idx)
    {
        $this->idx = $idx;
    }

    /**
     * 32/64 bit random integer.
     *
     * @return int
     */
    public function getId()
    {
        return $this->idx;
    }

    /**
     * Record start of timer in microseconds.
     *
     * If timer is already running, don't record another start.
     */
    public function startTimer()
    {
        tideways_span_timer_start($this->idx);
    }

    /**
     * Record stop of timer in microseconds.
     *
     * If timer is not running, don't record.
     */
    public function stopTimer()
    {
        tideways_span_timer_stop($this->idx);
    }

    /**
     * Annotate span with metadata.
     *
     * @param array<string,scalar>
     */
    public function annotate(array $annotations)
    {
        tideways_span_annotate($this->idx, $annotations);
    }
}

tideways_enable(TIDEWAYS_FLAGS_NO_HIERACHICAL);

$span = TwExtensionSpan::createSpan('php');
$span->annotate(array('title' => 'something'));
$span->startTimer();

$iterations = 1;
$skipped = 1000;

$span->annotate(array('iterations' => $iterations));
$span->annotate(array('skipped' => $skipped));
$span->annotate(array('found normal quest' => 0));

$span->stopTimer();

tideways_disable();

print_spans(tideways_get_spans());

var_dump($iterations, $skipped);
--EXPECTF--
app: 1 timers - cpu=%d
php: 1 timers - found normal quest=0 iterations=1 skipped=1000 title=something
int(1)
int(1000)
