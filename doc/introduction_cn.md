# 高可用分布式协调组件

**nexus**是一个基于[Raft协议](https://raft.github.io)的高可用数据库，并支持数据变更通知和分布式锁。可用于存储关键的元数据以及大型服务的协调工作。  

## nexus的用户

nexus原本是[Galaxy集群管理系统](https://github.com/baidu/galaxy)的一个组件，用于寻址、元数据存储和分布式锁服务，提供类似*Google Chubby*的服务。之后也被用于包括[Tera分布式表格数据库](https://github.com/baidu/tera)等项目中。

nexus可以用来管理集群服务，提供naming、元数据存储以及集群单例保证等功能。[这里](doc/usage_cn.md)列出了一些常见的使用方法，可以用做参考。

## 安装与使用

使用`build.sh`来在linux系统上获取nexus的各项依赖，或者参考`Makefile`开头列出的依赖设置环境变量或修改`Makefile`中的值，已在g++和clang++上得到验证。

`sandbox`目录中存放了一系列用于在本机搭建测试集群的脚本以及老版本的客户端脚本。
* `start_all.sh`用于启动一个包含5个节点的测试集群，并生成flag文件用于客户端脚本。
* `stop_all.sh`用于停止测试集群，`clean_all.sh`用于清理测试集群的日志和数据。
* `start_one.sh`以及`stop_one.sh`用于起停单个节点。
* `show_cluster.sh`用于展示集群的状态。
* `ins_shell.sh`会启动一个用脚本包装c++实现的nexus客户端。
* `run_sample.sh`用于启动一个写入10万次后读取10万次的测试任务。

为方便部署和使用，除了最初设计的脚本形式的客户端外，新增了一个纯c++版本的[客户端](src/client/ncli.cc)，可以用于测试以及对线上集群的操作，也可以作为使用nexus SDK的参考。

`control.sh`可以用于上线时控制单个节点的状态，设置正确的环境变量之后，可以控制起停以及检查节点的状态，方便自动上线。

## API和集成开发

目前nexus提供了c++和python两种语言的SDK，方便日常使用和web界面的开发。

* c++的用户可以通过`make install_sdk`来将sdk发布到`PRFIX`指定的路径。[这里](doc/cxx_api_cn.md)是c++ API的手册可供参考。
* python用户可以通过`make python`来发布python的库。[这里](doc/python_api_cn.md)是python的API手册。

## 编译依赖

nexus依赖了一个基于protobuf的RPC框架以及其他一些基础的第三方库。编译集群二进制以及客户端需要以下依赖：
* [snappy](https://github.com/google/snappy) 1.1.1及以上
* [boost](http://www.boost.org/) 1.57.0及以上
* [GFlags](https://github.com/gflags/gflags) 2.1.2及以上
* [Google Protocal Buffers](https://github.com/google/protobuf) 2.6.1
* [sofa-pbrpc](https://github.com/baidu/sofa-pbrpc) 1.1.3及以上

如果要编译单元测试，需要使用如下依赖：
* [Google Test](https://github.com/google/googletest) 1.7.0及以上

Raft协议中定义了关于日志压缩的方式。nexus实现日志压缩是通过修改了LevelDB在compact时候的行为来实现的。修改之后的代码放在`thirdparty/leveldb`中。

## 联系我们

我们十分欢迎包括issue、pr在内的任何形式的contribution。如果对我们的项目感兴趣，或者想给我们提意见，可以通过[opensearch](mailto:opensearch@baidu.com)联系我们

