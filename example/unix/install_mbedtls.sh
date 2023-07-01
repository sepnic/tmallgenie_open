#!/bin/bash

echo "Installing mbedtls"

MBEDTLS_VER=2.28.3

if [ ! -e mbedtls-$MBEDTLS_VER.tar.gz ]; then
  wget -T 10 -t 3 \
    https://github.com/Mbed-TLS/mbedtls/archive/refs/tags/mbedtls-$MBEDTLS_VER.tar.gz || exit 1;
fi

if [ ! -e mbedtls ]; then
  tar -xovzf mbedtls-$MBEDTLS_VER.tar.gz || exit 1
  mv mbedtls-mbedtls-$MBEDTLS_VER mbedtls
  mkdir -p mbedtls/install
fi

cd mbedtls/install

cmake -DENABLE_TESTING=Off ..
make

cd -
