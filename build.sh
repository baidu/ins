#!/bin/bash

PROJECT_DIR=$(cd $(dirname $0); pwd)
DEPS_SRC=${PROJECT_DIR}/thirdparty
DEPS_DST=${PROJECT_DIR}/depends
FLAGS_DIR=${DEPS_DST}/.flags

mkdir -p ${DEPS_SRC} ${DEPS_DST} ${FLAGS_DIR}

function build_snappy() {
    if [ ! -f "${FLAGS_DIR}/snappy_1_1_1" ] \
        || [ ! -f "${DEPS_DST}/lib/libsnappy.a" ] \
        || [ ! -f "${DEPS_DST}/include/snappy.h" ]; then
        cd ${DEPS_SRC}
        wget -O snappy-1.1.1.tar.gz http://pkgs.fedoraproject.org/repo/pkgs/snappy/snappy-1.1.1.tar.gz/8887e3b7253b22a31f5486bca3cbc1c2/snappy-1.1.1.tar.gz
        tar zxf snappy-1.1.1.tar.gz
        cd -
        cd ${DEPS_SRC}/snappy-1.1.1
        ./configure --prefix=${DEPS_DST} --disable-shared --with-pic
        make -j 4
        make install
        cd -
        touch "${FLAGS_DIR}/snappy_1_1_1"
    fi
}

function build_boost() {
    if [ ! -f "${FLAGS_DIR}/boost_1_57_0" ] \
        || [ ! -d "${DEPS_DST}/boost_1_57_0/boost" ]; then
        cd ${DEPS_SRC}
        wget -O boost_1_57_0.tar.gz http://kent.dl.sourceforge.net/project/boost/boost/1.57.0/boost_1_57_0.tar.gz
        tar zxf boost_1_57_0.tar.gz
        rm -rf ${DEPS_DST}/boost_1_57_0
        mv boost_1_57_0 ${DEPS_DST}
        cd -
        touch "${FLAGS_DIR}/boost_1_57_0"
    fi
}

function build_gflags() {
    if [ ! -f "${FLAGS_DIR}/gflags_2_1_2" ] \
        || [ ! -f "${DEPS_DST}/lib/libgflags.a" ] \
        || [ ! -d "${DEPS_DST}/include/gflags" ]; then
        cd ${DEPS_SRC}
        if ! which cmake ; then
            wget -O CMake-3.2.1.tar.gz https://github.com/Kitware/CMake/archive/v3.2.1.tar.gz
            tar zxf CMake-3.2.1.tar.gz
            cd -
            cd ${DEPS_SRC}/CMake-3.2.1
            ./configure --prefix="${DEPS_DST}"
            make -j 4
            make install
            cd -
            cd ${DEPS_SRC}
        fi
        wget -O gflags-2.1.2.tar.gz https://github.com/gflags/gflags/archive/v2.1.2.tar.gz
        tar xf gflags-2.1.2.tar.gz
        cd -
        cd ${DEPS_SRC}/gflags-2.1.2
        cmake -DGFLAGS_NAMESPACE=google -DCMAKE_CXX_FLAGS=-fPIC -DCMAKE_INSTALL_PREFIX="${DEPS_DST}"
        make -j 4
        make install
        cd -
        touch "${FLAGS_DIR}/gflags_2_1_2"
    fi
}

function build_protobuf() {
    if [ ! -f "${FLAGS_DIR}/protobuf_2_6_1" ] \
        || [ ! -f "${DEPS_DST}/lib/libprotobuf.a" ] \
        || [ ! -d "${DEPS_DST}/include/google/protobuf" ]; then
        cd ${DEPS_SRC}
        wget -O protobuf-2.6.1.tar.gz https://github.com/google/protobuf/releases/download/v2.6.1/protobuf-2.6.1.tar.gz
        tar zxf protobuf-2.6.1.tar.gz
        cd -
        cd ${DEPS_SRC}/protobuf-2.6.1
        ./configure --prefix=${DEPS_DST} --disable-shared --with-pic
        make -j 4
        make install
        cd -
        touch "${FLAGS_DIR}/protobuf_2_6_1"
    fi
}

function build_sofa_pbrpc() {
    if [ ! -f "${FLAGS_DIR}/sofa-pbrpc_1_1_3" ] \
        || [ ! -f "${DEPS_DIR}/lib/libsofa-pbrpc.a" ] \
        || [ ! -d "${DEPS_DIR}/include/sofa/pbrpc" ]; then
        cd ${DEPS_SRC}
        wget -O sofa-pbrpc-1.1.3.tar.gz https://github.com/baidu/sofa-pbrpc/archive/v1.1.3.tar.gz
        tar zxf sofa-pbrpc-1.1.3.tar.gz
        cd -
        cd ${DEPS_SRC}/sofa-pbrpc-1.1.3
        sed -i '/BOOST_HEADER_DIR=/ d' depends.mk
        sed -i '/PROTOBUF_DIR=/ d' depends.mk
        sed -i '/SNAPPY_DIR=/ d' depends.mk
        echo "BOOST_HEADER_DIR=${DEPS_DST}/boost_1_57_0" >> depends.mk
        echo "PROTOBUF_DIR=${DEPS_DST}" >> depends.mk
        echo "SNAPPY_DIR=${DEPS_DST}" >> depends.mk
        echo "PREFIX=${DEPS_DST}" >> depends.mk
        make -j 4
        make install
        cd -
        touch "${FLAGS_DIR}/sofa-pbrpc_1_1_3"
    fi
}

function build_gtest() {
    if [ ! -f "${FLAGS_DIR}/gtest_1_7_0" ] \
        || [ ! -f "${DEPS_DST}/lib/libgtest.a" ] \
        || [ ! -d "${DEPS_DST}/include/gtest" ]; then
        cd ${DEPS_SRC}
        wget -O gtest_1_7_0.tar.gz https://github.com/google/googletest/archive/release-1.7.0.tar.gz
        tar zxf gtest_1_7_0.tar.gz
        cd googletest-release-1.7.0
        sed -i 's/-Wno-missing-field-initializers//g' cmake/internal_utils.cmake
        cmake .
        make -j 4
        cp -af lib*.a ${DEPS_DST}/lib
        cp -af include/gtest ${DEPS_DST}/include
        cd -
        touch "${FLAGS_DIR}/gtest_1_7_0"
    fi
}

function main() {
    echo "building snappy..."
    build_snappy
    echo "snappy done."
    echo "building boost..."
    build_boost
    echo "boost done."
    echo "building gflags..."
    build_gflags
    echo "gflags done."
    echo "building protobuf..."
    build_protobuf
    echo "protobuf done."
    echo "building sofa-pbrpc..."
    build_sofa_pbrpc
    echo "sofa-pbrpc done."
    echo "building gtest..."
    build_gtest
    echo "gtest done."
}

main "$@"

