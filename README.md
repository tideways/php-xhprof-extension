# Tideways XHProf Extension

Home of the `tideways_xhprof` extension - a hierarchical Profiler for PHP.

**This extensions is not compatible with our Tideways service. Are you looking
for `tideways` Extension to use with tideways.com?** [Download here](https://tideways.io/profiler/downloads).

This PHP extension is a complete, modernized open-source rewrite of the
original XHProf extension, with a new core datastructure and specifically
optimized for PHP 7. The result is an XHProf data-format compatible extension
with a much reduced overhead in the critical path that you are profiling.

We are committed to provide support for this extension and port it to as many
platforms as possible.

**Note:** The public API is not compatible to previous xhprof extensions and
forks, but function names are different. Only the data format is compatible.

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
- OS: Linux, MacOS, Windows ([Download DLLs](https://ci.appveyor.com/project/tideways/php-profiler-extension))
- Architectures: x64/amd64, x86, ARM, PowerPC
- Non-Threaded (NTS) or Threaded (ZTS) support

## Installation

You can install the extension from source:

    phpize
    ./configure
    make
    sudo make install

Configure the extension to load with this PHP INI directive:

    extension=tideways_xhprof.so

Restart Apache or PHP-FPM.

### Download Pre-Compiled Binaries

We pre-compile binaries for Linux AMD64 and for Windows. See the [releases page for the downloads](https://github.com/tideways/php-xhprof-extension/releases) for each tagged version.

The Debian and RPM packages install the PHP extension to `/usr/lib/tideways_xhprof` and doesn't automatically put it into your PHP installation extension directory.
You should link the package by full path for a simple installation:

    extension=/usr/lib/tideways_xhprof/tideways_xhprof-7.3.so

## Usage

The API is not compatible to previous xhprof extensions and forks,
only the data format is compatible:

```php
<?php

tideways_xhprof_enable();

my_application();

file_put_contents(
    sys_get_temp_dir() . DIRECTORY_SEPARATOR . uniqid() . '.myapplication.xhprof',
    serialize(tideways_xhprof_disable())
);

```

By default only wall clock time is measured, you can enable
there additional metrics passing the `$flags` bitmask to `tideways_xhprof_enable`:

```php
<?php

tideways_xhprof_enable(TIDEWAYS_XHPROF_FLAGS_MEMORY | TIDEWAYS_XHPROF_FLAGS_CPU);

my_application();

file_put_contents(
    sys_get_temp_dir() . DIRECTORY_SEPARATOR . uniqid() . '.myapplication.xhprof',
    serialize(tideways_xhprof_disable())
);
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

When `TIDEWAYS_XHPROF_FLAGS_MEMORY_ALLOC` flag is set, the following additional values are set:
- `mem.na` The sum of the number of all allocations in this function.
- `mem.nf` The sum of the number of all frees in this function.
- `mem.aa` The amount of allocated memory.

If `TIDEWAYS_XHPROF_FLAGS_MEMORY_ALLOC_AS_MU` is set, `TIDEWAYS_XHPROF_FLAGS_MEMORY_ALLOC` is activated
and, if `TIDEWAYS_XHPROF_FLAGS_MEMORY_MU` is not set, `mem.aa` is additionally returned in `mu`.

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
        "cpu" => 400,
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

## Clock Sources

Any Profiler needs timer functions to calculate the duration of a function call
and the `tideways_xhprof` extension is no different. On Linux you can collect
timing information through various means. The classic, most simple one is the
function `gettimeofday`, which PHP uses when you call `microtime()`. This function
is slower compared to other mechanisms that the kernel provides:

- `clock_gettime(CLOCK_MONOTONIC)` returns a monotonically increasing number
  (not a timestamp) at very high precision and much faster than
  `gettimeofday()`. It is the preferred and recommended API to get high precision timestamps.
  On Xen based virtualizations (such as AWS) this call is much slower than on bare-metal
  or other virtualizations ([Blog post](https://blog.packagecloud.io/eng/2017/03/08/system-calls-are-much-slower-on-ec2/))
- TSC (Time Stamp Counter) API is accessible in C using inline assembler. It
  was the timing API that the original XHProf extension used and it is
  generally very fast, however depending on the make and generation of the CPU
  might not be synchronized between cores. On modern CPUs it is usually good to
  use without having to force the current process to a specific CPU.

Tideways on Linux defaults to using `clock_gettime(CLOCK_MONOTONIC)`, but if
you are running on Xen based virtualization, you could try to reduce the
overhead by setting `tideways.clock_use_rdtsc=1" in your PHP.ini.
