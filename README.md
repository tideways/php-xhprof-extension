# Qafoo Profiler PHP Extension

The Profiler extension contains functions for finding performance
bottlenecks in PHP code. It hooks into the Zend Engine with three different
profiling modes to collect information:

- Hierachical Profiling mode

    Collect profiling information for parent ==> child pairs of functions and
    methods. It records the number of calls from the parent to the child and
    the summed duration of these calls.

- Layer Profiling mode 

    Collect number of calls and duration to internal PHP functions aggregated
    into layers such as database, cache or http.

- Sample Profiling mode 

    Collect a list of full stack traces every 0.1 second of a request.

The extension is one core piece of functionality for the [Qafoo Profiler
Platform](https://qafoolabs.com). The platform solves the problem of
efficiently collecting, aggregating and analyzing the profiling data when
running the Profiler in production. 

## Installation

You can install the Qafoo Profiler extension from source or by downloading the
pre-compiled binaries from the [Qafoo
Profiler](https://qafoolabs.com/profiler/downloads) website.

Building from source is straightforward:

    cd profiler-extension
    phpize
    ./configure
    make
    sudo make install

Afterwards you need to enable the extension in your php.ini:

    extension=qafooprofiler.ini

