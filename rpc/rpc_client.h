// Copyright (c) 2014, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yanshiguang02@baidu.com

#ifndef  RPC_CLIENT_H_
#define  RPC_CLIENT_H_

#include <algorithm>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <sofa/pbrpc/pbrpc.h>
#include "common/mutex.h"
#include "common/thread_pool.h"
#include "common/logging.h"

namespace galaxy {
namespace ins {
class RpcClient {
public:
    RpcClient(int work_thread = 4, int callback_thread = 4) {
        // 定义 client 对象，一个 client 程序只需要一个 client 对象
        // 可以通过 client_options 指定一些配置参数，譬如线程数、流控等
        sofa::pbrpc::RpcClientOptions options;
        options.max_pending_buffer_size = 10;
        options.work_thread_num = work_thread;
        options.callback_thread_num = callback_thread;
        rpc_client_ = new sofa::pbrpc::RpcClient(options);
    }
    ~RpcClient() {
        delete rpc_client_;
    }
    template <class T>
    bool GetStub(const std::string server, T** stub) {
        MutexLock lock(&map_lock_);
        StubMap::iterator it = stub_map_.find(server);
        if (it != stub_map_.end()) {
            *stub = static_cast<T*>(it->second);
            return true;
        }
        sofa::pbrpc::RpcChannel* channel = NULL;
        HostMap::iterator jt = host_map_.find(server);
        if (jt != host_map_.end()) {
            channel = jt->second;
        } else {
            // 定义 channel，代表通讯通道，每个服务器地址对应一个 channel
            // 可以通过 channel_options 指定一些配置参数
            sofa::pbrpc::RpcChannelOptions channel_options;
            channel = new sofa::pbrpc::RpcChannel(rpc_client_, server, channel_options);
            host_map_[server] = channel;
        }
        *stub = new T(channel);
        stub_map_[server] = *stub;
        return true;
    }
    template <class Stub, class Request, class Response, class Callback>
    bool SendRequest(Stub* stub, void(Stub::*func)(
                    google::protobuf::RpcController*,
                    const Request*, Response*, Callback*),
                    const Request* request, Response* response,
                    int32_t rpc_timeout, int retry_times) {
        // 定义 controller 用于控制本次调用，并设定超时时间（也可以不设置，缺省为10s）
        sofa::pbrpc::RpcController controller;
        controller.SetTimeout(rpc_timeout * 1000L);
        for (int32_t retry = 0; retry < retry_times; ++retry) {
            (stub->*func)(&controller, request, response, NULL);
            if (controller.Failed()) {
                if (controller.ErrorCode() == sofa::pbrpc::RPC_ERROR_RESOLVE_ADDRESS) {
                    // 重新解析DNS
                    MutexLock lock(&map_lock_);
                    const StubMap::const_iterator it = std::find_if(
                            stub_map_.begin(), stub_map_.end(),
                            boost::bind(&StubMap::value_type::second, _1) == stub);
                    const HostMap::const_iterator jt = host_map_.find(it->first);
                    jt->second->Init();
                }
                if (retry < retry_times - 1) {
                    LOG(WARNING, "Send failed, retry ...\n");
                    usleep(1000000);
                } else {
                    LOG(WARNING, "SendRequest fail: %s\n", controller.ErrorText().c_str());
                }
            } else {
                return true;
            }
            controller.Reset();
        }
        return false;
    }
    template <class Stub, class Request, class Response, class Callback>
    void AsyncRequest(Stub* stub, void(Stub::*func)(
                    google::protobuf::RpcController*,
                    const Request*, Response*, Callback*),
                    const Request* request, Response* response,
                    boost::function<void (const Request*, Response*, bool, int)> callback,
                    int32_t rpc_timeout, int retry_times) {
        (void)retry_times;
        sofa::pbrpc::RpcController* controller = new sofa::pbrpc::RpcController();
        controller->SetTimeout(rpc_timeout * 1000L);
        google::protobuf::Closure* done = 
            sofa::pbrpc::NewClosure(&RpcClient::template RpcCallback<Stub, Request, Response, Callback>,
                                          this, stub, controller, request, response, callback);
        (stub->*func)(controller, request, response, done);
    }
    template <class Stub, class Request, class Response, class Callback>
    static void RpcCallback(RpcClient* self,
                            Stub* stub, sofa::pbrpc::RpcController* rpc_controller,
                            const Request* request,
                            Response* response,
                            boost::function<void (const Request*, Response*, bool, int)> callback) {
        bool failed = rpc_controller->Failed();
        int error = rpc_controller->ErrorCode();
        if (failed || error) {
            if (error != sofa::pbrpc::RPC_ERROR_SEND_BUFFER_FULL) {
                LOG(WARNING, "RpcCallback: %s\n", rpc_controller->ErrorText().c_str());
            }
            if (error == sofa::pbrpc::RPC_ERROR_RESOLVE_ADDRESS) {
                MutexLock lock(&self->map_lock_);
                const StubMap::const_iterator it = std::find_if(
                        self->stub_map_.begin(), self->stub_map_.end(),
                        boost::bind(&StubMap::value_type::second, _1) == stub);
                const HostMap::const_iterator jt = self->host_map_.find(it->first);
                jt->second->Init();
            }
        }
        delete rpc_controller;
        callback(request, response, failed, error);
    }
private:
    sofa::pbrpc::RpcClient* rpc_client_;
    typedef std::map<std::string, sofa::pbrpc::RpcChannel*> HostMap;
    typedef std::map<std::string, void*> StubMap;
    HostMap host_map_;
    StubMap stub_map_;
    Mutex map_lock_;
};

} // namespace ins
} // namespace galaxy

#endif  // RPC_CLIENT_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
