# Tideways PHP Profiler Extension

[![Build Status](https://travis-ci.org/tideways/php-profiler-extension.svg?branch=master)](https://travis-ci.org/tideways/php-profiler-extension)

The Profiler extension contains functions for finding performance bottlenecks
in PHP code. The extension is one core piece of functionality for the [Tideways
Profiler Platform](https://tideways.io). It solves the problem of efficiently
collecting, aggregating and analyzing the profiling data when running a
Profiler in production.

## Requirements

- PHP 5.3, 5.4, 5.5 or 5.6
- cURL and PCRE Dev Headers (`apt-get install libcurl4-openssl-dev libpcre3-dev')
- Tested with Linux i386, amd64 architectures

## Installation

You can install the Tideways extension from source or download
pre-compiled binaries from the [Tideways Downloads](https://tideways.io/profiler/downloads) page.

Building from source is straightforward:

    git clone https://github.com/tideways/php-profiler-extension.git
    cd php-profiler-extension
    phpize
    ./configure
    make
    sudo make install

You also need the latest ``Tideways.php`` if you want to use the Profiler in combination with our daemon and UI.
[Download the file from Github](https://github.com/tideways/profiler/releases). Put this file into your
extension directory. You can find the location by calling:

    $ php -r 'echo ini_get("extension_dir");'
    $ cp Tideways.php /path/to/php/lib

Afterwards you need to enable the extension in your php.ini:

    extension=tideways.so
    tideways.api_key=set your key

**Important:** If you don't want to use Tideways platform you should also add the following ini configuration to
your php.ini:

    extension=tideways.so
    tideways.auto_prepend_library=0

## Documentation

You can find the documentation on the [Tidways Profiler
website](https://tideways.io/profiler/docs/setup/profiler-php-pecl-extension).

