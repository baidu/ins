#include <gflags/gflags.h>

DEFINE_string(cluster_members, "", "cluster members , e.g. abc.com:1234,def.com:3456");
DEFINE_int32(ins_port, 8868, "listen port of ins-node");
DEFINE_string(ins_data_dir, "data", "local directory which store pesistent information");
