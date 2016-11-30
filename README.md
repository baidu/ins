# iNexus
iNexus is a high available key-value store, which can be used to coordinate large scale services. (based on Raft protocol)


iNexus (简称ins) 是一个基于Raft协议实现的高可用的分布式Key-Value数据库，支持数据变更通知（Watch）和分布式锁，可用于大型分布式系统的协调工作

# 简介
iNexus原本是Galaxy集群管理系统的一个组件，它的产生源于我们的业务需要一个类似于Google Chubby一样的组件。它能提供分布式naming、lock服务，并能存储一定规模的元数据(百GB的级别)。

iNexus的理论基础是Raft一致性协议，这个协议主要解决了多个节点之间的数据变更一致性同步，从而可以由多个节点构成一个高可用的数据存储集群(半数以下节点宕机不影响服务)。iNexus实现了Raft协议的主要部分，但目前还不支持动态改变集群membership。Raft的paper只论述了集群选主和数据一致性复制，iNexus结合实际需求，又加上了Watch(事件主动通知)和Lock(分布式锁)。


[![Build Status](https://travis-ci.org/baidu/ins.svg?branch=master)](https://travis-ci.org/baidu/ins)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/10966/badge.svg)](https://scan.coverity.com/projects/10966)

# API使用说明
https://github.com/baidu/ins/wiki

# build for Baiduer
* comake2 -UB
* comake2
* make

#Ubuntu系统下的编译
* [./ubuntu_build.sh ](https://github.com/fxsjy/ins/blob/master/ubuntu_build.sh)

# 快速体验
  只需要*一台机器*，通过多个进程模拟分布式	
  
  *免编译体验* -- 采用statifier静态化的预编译版本，在主流64位Linux发行版上都能直接运行：	
  *下载* -- [ins_prebuild.tar.gz] (http://github.com/fxsjy/thirdparty/raw/master/ins_prebuild.tar.gz)		
	
	cd sandbox
	./start_all.sh  (启动本机5个进程监听不同端口，模拟一个集群）
	./ins_shell.sh  （进入命令行管理界面）
		
	galaxy ins> help
	  show [ show cluster ]
	  put (key) (value) [ update the data ] 
	  get (key) [read the data by key ]
	  delete (key) [remove the data by key]
	  scan (start-key) (end-key) [scan from start-key to end-key(excluded)]
	  watch (key) [event will be triggered once value changed or deleted]
	  lock (key) [lock on specific key]
	  enter quit to exit shell
  ![Screenshot](https://raw.githubusercontent.com/fxsjy/thirdparty/master/ins_screen_shot.png)
  
# 读写性能 (multiple clients， 5 cluster-nodes in LAN)
* 53200 read per second
* 9100 write per second

# use the SDK (client library)
* C++ 客户端库编译：make install_sdk
* Python 客户端编译： make python
* include [ins_sdk.h](https://github.com/fxsjy/ins/blob/master/sdk/ins_sdk.h) , link the static library libins_sdk.a
* usage example: https://github.com/fxsjy/ins/blob/master/sdk/sample.cc


# 通用的编译方式
* read [.travis.yml](https://github.com/baidu/ins/blob/master/.travis.yml) and install the depends libs
* make && make install
* 如果只编译sdk, 不依赖leveldb

