#!/bin/bash

echo "Installing mbedtls"

if [ ! -e mbedtls-2.16.12.tar.gz ]; then
  wget -T 10 -t 3 \
    https://github.com/Mbed-TLS/mbedtls/archive/refs/tags/mbedtls-2.16.12.tar.gz || exit 1;
fi

if [ ! -e mbedtls ]; then
  tar -xovzf mbedtls-2.16.12.tar.gz || exit 1
  mv mbedtls-mbedtls-2.16.12 mbedtls
  mkdir -p mbedtls/install
fi

cd mbedtls/install

cmake -DENABLE_TESTING=Off ..
make

cd -
