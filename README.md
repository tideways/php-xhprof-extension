# Qafoo Profiler PHP Extension

The Profiler extension contains functions for finding performance bottlenecks
in PHP code.The extension is one core piece of functionality for the [Qafoo
Profiler Platform](https://qafoolabs.com). The platform solves the problem of
efficiently collecting, aggregating and analyzing the profiling data when
running the Profiler in production.

## Requirements

- PHP 5.3, 5.4, 5.5 or 5.6
- cURL and PCRE Dev Headers (`apt-get install libcurl4-openssl-dev libpcre3-dev')
- Tested with Linux i386, amd64 architectures

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

    extension=qafooprofiler.so

## Documentation

You can find the documentation on the [QafooLabs Profiler website](https://qafoolabs.com/profiler/docs/setup/profiler-php-pecl-extension).
