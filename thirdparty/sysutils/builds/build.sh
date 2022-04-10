#!/bin/bash

set -x
set -e

TOP_DIR=${PWD}/..
CUR_DIR=${PWD}
OUTPUT_DIR=${PWD}/out
OUTPUT_LIB_DIR=${OUTPUT_DIR}/lib
OUTPUT_BIN_DIR=${OUTPUT_DIR}/bin

# build libsysutils
echo "Building sysutils library"
mkdir -p ${OUTPUT_LIB_DIR}
cd ${OUTPUT_LIB_DIR}
cmake ${TOP_DIR}
make

# build test
echo "Building sysutils test"
mkdir -p ${OUTPUT_BIN_DIR}
cd ${OUTPUT_BIN_DIR}
cmake ${TOP_DIR}/test
make

# build android jni
cd ${CUR_DIR}
ndk-build -C android/jni
