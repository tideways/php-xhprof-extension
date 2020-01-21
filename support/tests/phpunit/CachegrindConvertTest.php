<?php

namespace Tideways\Xhprof\Tests;

use PHPUnit\Framework\TestCase;
use Tideways\Xhprof\CachegrindConverter;

class CachegrindConverterTest extends TestCase
{
    public function testConvert()
    {
        $data = [
            'main()' => ['wt' => 100, 'ct' => 1],
            'main()==>foo' => ['wt' => 30, 'ct' => 1],
            'main()==>bar' => ['wt' => 30, 'ct' => 1],
            'foo==>baz' => ['wt' => 25, 'ct' => 10],
        ];

        $converter = new CachegrindConverter();
        $output = $converter->convertToCachegrind($data);

        $this->assertEquals(<<<CGT
            version: 1
            creator: tideways_xhprof
            cmd: php
            part: 1
            positions: line

            events: Time Memory

            fl=(1) php:internal
            fn=(1) baz
            1 25 0

            fl=(1) php:internal
            fn=(2) foo
            1 5 0
            cfl=(1)
            cfn=(1) baz
            calls=1 0 0
            1 25 10

            fl=(1) php:internal
            fn=(3) bar
            1 30 0

            fl=(1) php:internal
            fn=(4) main()
            1 40 0
            cfl=(1)
            cfn=(2) foo
            calls=1 0 0
            1 30 1
            cfl=(1)
            cfn=(3) bar
            calls=1 0 0
            1 30 1

            summary: 100 0

            CGT, $output);
    }
}
