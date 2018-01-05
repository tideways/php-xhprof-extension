# Tideways XHProf Extension

Home of the `tideways_xhprof` extension.

**Looking for `tideways` Extension to report to tideways.io?** [Go here](https://tideways.io/profiler/downloads).
**Why did we rename the extension?** [Blog post here](https://tideways.io/profiler/blog/releasing-new-tideways-xhprof-extension).

This PHP extension is a complete, modernized open-source rewrite of the
original XHProf extension, with a new core datastructure and specifically
optimized for PHP 7. The result is an XHProf data-format compatible extension
with a much reduced overhead in the critical path that you are profiling.

The code for this extension is extracted from the [main Tideways
extension](https://tideways.io) as we are moving to a new extension with
incompatible data-format.

We are committed to provide support for this extension and port it to as many
platforms as possible.

**Note:** The public API is not compatible to previous xhprof extensions and
forks, as function names are different. Only the data format is compatible.

## About tideways and tideways_xhprof Extensions

This repository now contains an extension by the name of `tideways_xhprof`,
which only contains the XHProf related (Callgraph) Profiler functionality.

Previously the `tideways` extension contained this functionality together with
other functionality used in our Software as a Service.

If you want to use the SaaS, the current approach is to fetch the code using
precompiled binaries and packages from our [Downloads
page](https://tideways.io/profiler/downloads).

## Requirements

- PHP >= 7.0
- OS: Linux, MacOS, Windows ([Download DDLs](https://ci.appveyor.com/project/tideways/php-profiler-extension))

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
