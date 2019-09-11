#!/bin/bash

set -e

PHP_VERSION=$1
EXTENSION="tideways_xhprof"

make distclean || true
/opt/php/php-${PHP_VERSION}/bin/phpize
CFLAGS="-O2" ./configure --with-php-config=/opt/php/php-${PHP_VERSION}/bin/php-config
make -j2

cp modules/${EXTENSION}.so modules/${EXTENSION}-${PHP_VERSION}.so
buildkite-agent artifact upload modules/${EXTENSION}-${PHP_VERSION}.so

REPORT_EXIT_STATUS=1 /opt/php/php-${PHP_VERSION}/bin/php run-tests.php -p /opt/php/php-${PHP_VERSION}/bin/php --show-diff -d extension=`pwd`/.libs/${EXTENSION}.so -q
