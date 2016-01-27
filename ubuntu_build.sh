#! /bin/bash

sudo apt-get install libboost-dev libsnappy-dev

git clone --depth=1 https://github.com/00k/protobuf >/dev/null
mv protobuf/protobuf-2.6.1.tar.gz .
tar zxf protobuf-2.6.1.tar.gz >/dev/null
cd protobuf-2.6.1
./configure --disable-shared --with-pic  >/dev/null
make -j4 >/dev/null
sudo make install
cd -

sudo apt-get install zlib1g-dev
git clone https://github.com/baidu/sofa-pbrpc.git ./thirdparty/sofa-pbrpc
cd thirdparty/sofa-pbrpc && make -j4 && make install
cd -

wget https://github.com/gflags/gflags/archive/v2.1.2.tar.gz
tar xf v2.1.2.tar.gz
cd gflags-2.1.2 && cmake -DGFLAGS_NAMESPACE=google -DCMAKE_CXX_FLAGS=-fPIC && make -j4 && sudo make install
cd -

make -j4
