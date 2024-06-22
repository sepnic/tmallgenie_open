#!/bin/bash

set -x
set -e

TOP_DIR=${PWD}
OUTPUT_DIR=${PWD}/out

# build libs
echo "Building library"
mkdir -p ${OUTPUT_DIR}/library
cd ${OUTPUT_DIR}/library
cmake ${TOP_DIR}/src
make

# build example
echo "Building example"
mkdir -p ${OUTPUT_DIR}/example
cd ${OUTPUT_DIR}/example
cmake ${TOP_DIR}/example/unix
make

