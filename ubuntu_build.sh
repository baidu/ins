#! /bin/bash

sudo apt-get install libboost-dev libsnappy-dev
sudo apt-get install zlib1g-dev

mkdir depends
script_name=`readlink -f $0`
base_dir=`dirname $script_name`

cd $base_dir/thirdparty
git clone --depth=1 https://github.com/00k/protobuf >/dev/null
mv protobuf/protobuf-2.6.1.tar.gz .
tar zxf protobuf-2.6.1.tar.gz >/dev/null
cd protobuf-2.6.1
./configure --disable-shared --with-pic --prefix $base_dir/depends >/dev/null
make -j4 >/dev/null
make install

cd $base_dir/thirdparty
git clone https://github.com/baidu/sofa-pbrpc.git ./sofa-pbrpc
sed -r 's|PROTOBUF_DIR=(.+)|PROTOBUF_DIR='"${base_dir}/depends"'|g' -i ./sofa-pbrpc/depends.mk
cd ./sofa-pbrpc && make -j4 && make install

cd $base_dir/thirdparty
wget https://github.com/gflags/gflags/archive/v2.1.2.tar.gz
tar xf v2.1.2.tar.gz
cd gflags-2.1.2 && cmake -DGFLAGS_NAMESPACE=google -DCMAKE_CXX_FLAGS=-fPIC -DCMAKE_INSTALL_PREFIX:PATH="$base_dir/depends" && make -j4 && make install

cd $base_dir
export PATH=$PATH:./depends/bin
make -j4
