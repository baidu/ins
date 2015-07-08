#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <assert.h>
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
DECLARE_int32(server_id);

static volatile bool s_quit = false;
static void SignalIntHandler(int /*sig*/){
    s_quit = true;
}

int main(int argc, char* argv[]) {
    google::ParseCommandLineFlags(&argc, &argv, true);
    std::string host_name;
    std::vector<std::string> members;
    boost::split(members, FLAGS_cluster_members,
                 boost::is_any_of(","), boost::token_compress_on);
    if (members.size() == 0) {
        LOG(FATAL, "cluster is empty , please check your configuration");
        return -1;
    }
    if (FLAGS_server_id < 1 || 
        FLAGS_server_id > static_cast<int32_t>(members.size())) {
        LOG(FATAL, "bad server_id: %d", FLAGS_server_id);
        return -1;
    }
    std::string server_id = members.at(FLAGS_server_id - 1); //offset -> real endpoint
    galaxy::ins::InsNodeImpl * ins_node = new galaxy::ins::InsNodeImpl(server_id, 
                                                                       members);
    sofa::pbrpc::RpcServerOptions options;
    sofa::pbrpc::RpcServer rpc_server(options);
    if (!rpc_server.RegisterService(static_cast<galaxy::ins::InsNode*>(ins_node))) {
        LOG(FATAL, "failed to register ins_node service");
        exit(-1);
    }

    if (!rpc_server.Start(server_id)) {
        LOG(FATAL, "failed to start server on %s", server_id.c_str());
        exit(-2);
    }
    signal(SIGINT, SignalIntHandler);
    signal(SIGTERM, SignalIntHandler);
    while (!s_quit) {
    	sleep(1);
    }
    return 0;
}
