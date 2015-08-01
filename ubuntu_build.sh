#! /bin/bash

sudo apt-get install libleveldb-dev
sudo apt-get install libboost-dev libsnappy-dev

PROTOBUF_VERSION=2.6.0
wget https://github.com/google/protobuf/releases/download/v$PROTOBUF_VERSION/protobuf-$PROTOBUF_VERSION.tar.gz
tar xf protobuf-$PROTOBUF_VERSION.tar.gz
cd protobuf-$PROTOBUF_VERSION && ./configure && make -j4 && sudo make install && sudo ldconfig
cd -

git clone https://github.com/google/snappy
cd snappy && sh ./autogen.sh && ./configure && make -j2 && sudo make install
cd -

sudo apt-get install zlib1g-dev
git clone https://github.com/baidu/sofa-pbrpc.git ./thirdparty/sofa-pbrpc
cd thirdparty/sofa-pbrpc && make -j4 && make install
cd -

wget https://github.com/gflags/gflags/archive/v2.1.2.tar.gz
tar xf v2.1.2.tar.gz
cd gflags-2.1.2 && cmake -DGFLAGS_NAMESPACE=google && make -j4 && sudo make install
cd -

make -j4
