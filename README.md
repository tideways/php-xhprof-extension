# Tideways XHProf Extension

This extension is a complete rewrite of the original XHProf extension, with a
new core datastructure and specifically optimized for PHP 7. The result is an
XHProf data-format compatible extension with a much reduced overhead in the
critical path that you are profiling.

The code for this extension is extracted from the Tideways main extension, so
that we can still provide the PHP community with a modern open-source successor
of the wide-spread XHProf codebase using the "old" xhprof data-format usable
by various open-source user interfaces.

## Requirements

- PHP >= 7.0
- OS: Linux only (at the moment)

## Installation

You can install the extension from source:

    phpize
    ./configure
    make
    sudo make install

Configure the extension to load with this PHP INI directive:

    extension=tideways_xhprof.so

Restart Apache or PHP-FPM.

## Usage

The API is not compatible to previous xhprof extensions and forks,
only the data format is compatible:

```php
<?php

tideways_xhprof_enable();

my_application();

$data = tideways_xhprof_disable();

file_put_contents("/tmp/profile.xhprof", serialize($data));
```

By default only wall clock time is measured, you can enable
there additional metrics passing the `$flags` bitmask to `tideways_xhprof_enable`:

```php
<?php

tideways_xhprof_enable(TIDEWAYS_XHPROF_FLAGS_MEMORY | TIDEWAYS_XHPROF_FLAGS_CPU);

my_application();

$data = tideways_xhprof_disable();
```

## Data-Format

The XHProf data format records performance data for each parent => child
function call that was made between the calls to `tideways_xhprof_enable` and
`tideways_xhprof_disable`. It is formatted as an array with the parent and child
function names as a key concatenated with ==> and an array value with 2 to 5 entries:

- `wt` The summary wall time of all calls of this parent ==> child function pair.
- `ct` The number of calls between this parent ==> child function pair.
- `cpu` The cpu cycle time of all calls of thi sparent ==> child function pair.
- `mu` The sum of increase in `memory_get_usage` for this parent ==> child function pair.
- `pmu` The sum of increase in `memory_get_peak_usage` for this parent ==> child function pair.

There is a "magic" function call called "main()" that represents the entry into
the profiling.  The wall time on this performance data describes the full
timeframe that the profiling ran.

Example:

```php
<?php

array(
    "main()" => array(
        "wt" => 1000,
        "ct" => 1,
        "cpu" => 900,
    ),
    "main()==>foo" => array(
        "wt" => 500,
        "ct" => 2,
        "cpu" => 200,
    ),
    "foo==>bar" => array(
        "wt" => 200,
        "ct" => 10,
        "cpu" => 100,
    ),
)
```
