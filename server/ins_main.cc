#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <algorithm>
#include <vector>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <sofa/pbrpc/pbrpc.h>
#include <gflags/gflags.h>
#include "common/logging.h"
#include "ins_node_impl.h"

DECLARE_string(cluster_members);
DECLARE_int32(ins_port);

static volatile bool s_quit = false;
static void SignalIntHandler(int sig){
    s_quit = true;
}

int main(int argc, char* argv[]) {
    google::ParseCommandLineFlags(&argc, &argv, true);
    std::string host_name;
    std::vector<std::string> members;
    galaxy::ins::GetHostName(&host_name);
    assert(!host_name.empty());
    std::string end_point = host_name + ":" + boost::lexical_cast<std::string>(FLAGS_ins_port);
    boost::split(members, FLAGS_cluster_members,
                 boost::is_any_of(","), boost::token_compress_on);
    galaxy::ins::InsNodeImpl * ins_node = new galaxy::ins::InsNodeImpl(end_point, members);
    
    sofa::pbrpc::RpcServerOptions options;
    sofa::pbrpc::RpcServer rpc_server(options);
    if (!rpc_server.RegisterService(static_cast<galaxy::ins::InsNode*>(ins_node))) {
        LOG(FATAL, "failed to register ins_node service");
        exit(-1);
    }

    if (!rpc_server.Start(end_point)) {
        LOG(FATAL, "failed to start server on %s", end_point.c_str());
        exit(-2);
    }
    signal(SIGINT, SignalIntHandler);
    signal(SIGTERM, SignalIntHandler);
    while (!s_quit) {
    	sleep(1);
    }
    return 0;
}
