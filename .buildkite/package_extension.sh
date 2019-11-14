#!/bin/bash

set -e

BASEDIR=`dirname $0`
BASEDIR=`dirname $BASEDIR`
PACKAGENAME="tideways-xhprof"
DESCRIPTION="tideways-xhprof is a modern XHProf fork built for PHP 7."
EXTENSION="tideways_xhprof"
VERSIONS=( "7.0" "7.1" "7.2" "7.3" "7.4" "7.1-zts" "7.2-zts" "7.3-zts" "7.4-zts" )
PACKAGES=( "deb" "rpm" )
ARCHITECTURE=`uname -m`
ARCHITECTURE=${ARCHITECTURE/686/386}

buildkite-agent artifact download "modules/*.so" .

mkdir packaging/dist -p
mkdir packaging/root/usr/lib/${EXTENSION} -p
mkdir packaging/root/usr/share/doc/${PACKAGENAME} -p

for VERSION in "${VERSIONS[@]}"
do
    if [ -f "/opt/php/php-${VERSION}/bin/php" ]; then
        EXTVERSION=`/opt/php/php-${VERSION}/bin/php -dextension=modules/${EXTENSION}-${VERSION}.so -r "echo phpversion(\"${EXTENSION}\");"`
    fi
done

mkdir packaging/tarball/${EXTENSION}-${EXTVERSION} -p

cp modules/*.so packaging/tarball/${EXTENSION}-${EXTVERSION}/
cp modules/*.so packaging/root/usr/lib/${EXTENSION}/

cp ${BASEDIR}/LICENSE packaging/tarball/${EXTENSION}-${EXTVERSION}/LICENSE || true
cp ${BASEDIR}/NOTICE packaging/tarball/${EXTENSION}-${EXTVERSION}/NOTICE || true

cp ${BASEDIR}/LICENSE packaging/root/usr/share/doc/${PACKAGENAME}/LICENSE || true
cp ${BASEDIR}/NOTICE packaging/root/usr/share/doc/${PACKAGENAME}/NOTICE || true

pushd .

cd packaging/tarball
tar czvf ../dist/${PACKAGENAME}-${EXTVERSION}-${ARCHITECTURE}.tar.gz .

popd

buildkite-agent artifact upload packaging/dist/${PACKAGENAME}-${EXTVERSION}-${ARCHITECTURE}.tar.gz

for PKG in "${PACKAGES[@]}"
do
    fpm --maintainer "support@tideways.com" \
        --url "https://tideways.com" \
        --description "${DESCRIPTION}" \
        --vendor "Tideways GmbH" \
        -f \
        -s dir \
        -t ${PKG} \
        -C "${BASEDIR}/packaging/root" \
        -n "${PACKAGENAME}" \
        -a "${ARCHITECTURE}" \
        -v "${EXTVERSION}" \
        .

    buildkite-agent artifact upload "${PACKAGENAME}*.${PKG}"
done
