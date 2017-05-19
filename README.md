# Nexus - Raft Protocol Based Cluster Coordinator

[![Build Status](https://travis-ci.org/baidu/ins.svg?branch=master)](https://travis-ci.org/baidu/ins)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/10966/badge.svg)](https://scan.coverity.com/projects/10966)
[![Documentation Status](https://img.shields.io/badge/中文文档-最新-brightgreen.svg)](doc/introduction_cn.md)

**nexus**, used to be known as *i name service(ins)*, is a high available key-value store to hold crucial meta data, which also support mechanism to coordinate large scale services.

nexus is based on [Raft protocol](https://raft.github.io/) to keep consistency between mulit-nodes, and provide key-value data access, distributed lock mechanism(lock) and event callback function(watch).

## Who Is Using Nexus

nexus was born to provide a *Google Chubby* style service in [Galaxy](https://github.com/baidu/galaxy), a cluster manager system. Now it supports several online services in Baidu, Inc., including [Galaxy](https://github.com/baidu/galaxy) and [Tera](https://github.com/baidu/tera).

For those who want to save their important data, who want to keep uniqueness in a cluster, or who want to manage large scale services, please refer to [this](doc/usage.md) for some classical usage.

## Installation and Usage

Use `build.sh` to solve dependency problem, or export proper environment variable to tell Makefile to find proper headers and libraries. All necessary dependencies are listed at the beginning of Makefile. Proved on g++ and clang++.

The `sandbox` directory holds a few scripts for starting a test cluster on localhost and old-fashioned client tools.
* Use `start_all.sh` to start a 5-nodes cluster on current server, and generate flagfile for other scripts
* Use `stop_all.sh` to stop all nodes, and `clean_all.sh` to stop cluster and clean up all data and log
* Use `stop_one.sh` and `start_one.sh` to temporarily disable and enable a node to check the cluster status
* Use `show_cluster.sh` to show the status of cluster
* Use `ins_shell.sh` for old-fasioned nexus client, which uses script to wrap a binary client
* Use `run_sample.sh` to start a 100k write times test and a 100k read times test

In addition to script version of client, we introduced a new C++ version client, providing more convenient functions for testing and online operations.

When deploying to online servers, please use `control.sh` for a single node to start/stop/restart or check the status of health.

## Integration and API

* For C++ user, use `make install_sdk` to export sdk to `PREFIX` dir. Please refer to [this document](doc/cxx_api.md) for more information
* For Python user, use `make python` to produce library. Please refer to [this document](doc/python_api.md) for more information

## Dependencies

To build the whole project, following dependencies must be meet:
* [snappy](https://github.com/google/snappy) 1.1.1 and above
* [boost](http://www.boost.org/) 1.57.0 and above
* [GFlags](https://github.com/gflags/gflags) 2.1.2 and above
* [Google Protocal Buffers](https://github.com/google/protobuf) 2.6.1
* [sofa-pbrpc](https://github.com/baidu/sofa-pbrpc) 1.1.3 and above

For unittest, following dependency must be meet:
* [Google Test](https://github.com/google/googletest) 1.7.0 and above

Raft defined log compaction function needs a slight change in LevelDB, the modified code is located in `thirdparty/leveldb`.

## Contributing

We welcome any kind of contributions. If there are any developing questions or complaints, please feel free to contact us: [opensearch](mailto:opensearch@baidu.com).

