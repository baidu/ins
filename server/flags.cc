#include <gflags/gflags.h>

DEFINE_string(cluster_members, "", "cluster members , e.g. abc.com:1234,def.com:3456");
DEFINE_int32(ins_port, 8868, "listen port of ins-node");
DEFINE_string(ins_data_dir, "data", "local directory which store pesistent information");
DEFINE_string(ins_binlog_dir, "binlog", "write-ahead log directory path");
DEFINE_string(ins_cmd, "", "the command of inc shell");
DEFINE_string(ins_key, "key", "");
DEFINE_string(ins_start_key, "", "start key of scan");
DEFINE_string(ins_end_key, "", "end key of scan (excluded)");
DEFINE_string(ins_value, "v123", "");
DEFINE_int32(max_cluster_size, 10, "maximum size of ins cluster");
DEFINE_int32(log_rep_batch_max, 500, "maximum batch size of log replication");
DEFINE_int32(replication_retry_timespan, 2000, "when replication fail, sleep a while before retry");
DEFINE_int32(elect_timeout_min, 150, "mininum timeout to make a new election");
DEFINE_int32(elect_timeout_max, 300, "maximum timeout to make a new election");
DEFINE_int64(session_expire_timeout, 6000000, "timeout for session expiration, 6 seconds in default");
