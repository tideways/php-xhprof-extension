<?php

namespace Tideways\Xhprof;

class CachegrindConverter
{
    private $aggregations = [];
    private $children = [];
    private $data = [];
    private $output = '';
    private $renderedCalls = [];
    private $num = 0;

    public function convertToCachegrind(array $data)
    {
        $this->aggregations = [];
        $this->children = [];
        $this->renderedCalls = [];
        $this->data = $data;
        $this->num = 0;

        $this->aggregations['main()'] = ['ct' => 1, 'wt' => $data['main()']['wt'], 'pmu' => $data['main()']['pmu'] ?? 0];

        foreach ($data as $callPair => $callInfo) {
            if ($callPair === "main()") {
                continue;
            }

            [$parent, $child] = explode('==>', $callPair);

            if (!isset($this->children[$parent])) {
                $this->children[$parent] = [];
            }
            if (!isset($this->children[$child])) {
                $this->children[$child] = [];
            }
            $this->children[$parent][] = $child;

            if (isset($this->aggregations[$child])) {
                $this->aggregations[$child]['wt'] += $callInfo['wt'] ?? 0;
                $this->aggregations[$child]['pmu'] += $callInfo['pmu'] ?? 0;
            } else {
                $this->aggregations[$child]['wt'] = $callInfo['wt'] ?? 0;
                $this->aggregations[$child]['pmu'] = $callInfo['pmu'] ?? 0;
            }

            if (isset($this->aggregations[$parent])) {
                $this->aggregations[$parent]['wt'] -= $callInfo['wt'] ?? 0;
                $this->aggregations[$parent]['pmu'] -= $callInfo['pmu'] ?? 0;
            } else {
                $this->aggregations[$parent]['wt'] = -1 * $callInfo['wt'] ?? 0;
                $this->aggregations[$parent]['pmu'] = -1 * $callInfo['pmu'] ?? 0;
            }
        }

        $this->output = <<<CGD
            version: 1
            creator: tideways_xhprof
            cmd: php
            part: 1
            positions: line


            CGD;

        $this->output .= "events: Time Memory\n\n";

        $this->renderCall('main()');

        $this->data["main()"]["wt"] = $this->data["main()"]["wt"] ?? 0;
        $this->data["main()"]["pmu"] = $this->data["main()"]["pmu"] ?? 0;

        $this->output .= "summary: {$this->data["main()"]["wt"]} {$this->data["main()"]["pmu"]}\n";

        return $this->output;
    }

    private function renderCall($call)
    {
        if (isset($this->renderedCalls[$call])) {
            return;
        }

        $callInfo = $this->aggregations[$call];
        $this->renderedCalls[$call] = true;

        foreach ($this->children[$call] as $child) {
            $this->renderCall($child);
        }

        $this->num++;
        $this->aggregations[$call]['num'] = $this->num;

        $this->output .= "fl=(1) php:internal\n";
        $this->output .= "fn=({$this->num}) {$call}\n";
        $this->output .= "1 {$callInfo['wt']} {$callInfo['pmu']}\n";

        foreach ($this->children[$call] as $child) {
            $callPair = $call . "==>" . $child;
            $this->output .= "cfl=(1)\n";
            $this->output .= "cfn=({$this->aggregations[$child]['num']}) $child\n";
            $this->output .= "calls=1 0 0\n";
            $this->output .= "1 {$this->data[$callPair]['wt']} {$this->data[$callPair]['pmu']}\n";
        }

        $this->output .= "\n";
    }
}
