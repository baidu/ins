#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sofa/pbrpc/pbrpc.h>
#include "ins_node_impl.h"

static volatile bool s_quit = false;
static void SignalIntHandler(int sig){
    s_quit = true;
}

int main(int argc, char* argv[]) {
	sofa::pbrpc::RpcServerOptions options;
    sofa::pbrpc::RpcServer rpc_server(options);
    galaxy::ins::InsNode * ins_node = new galaxy::ins::InsNodeImpl();
    rpc_server.RegisterService(ins_node);
    rpc_server.Start("0.0.0.0:8899");
    signal(SIGINT, SignalIntHandler);
    signal(SIGTERM, SignalIntHandler);
    while (!s_quit) {
    	sleep(1);
    }
    return 0;
}
