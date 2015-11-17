#include <gflags/gflags.h>

DEFINE_string(cluster_members, "", "cluster members , e.g. abc.com:1234,def.com:3456");
DEFINE_int32(server_id, 1, "the offset in cluster members of this node");
DEFINE_string(ins_data_dir, "data", "local directory which store pesistent information");
DEFINE_string(ins_binlog_dir, "binlog", "write-ahead log directory path");
DEFINE_int32(max_cluster_size, 10, "maximum size of ins cluster");
DEFINE_int32(log_rep_batch_max, 500, "maximum batch size of log replication");
DEFINE_int32(replication_retry_timespan, 2000, "when replication fail, sleep a while before retry");
DEFINE_int32(elect_timeout_min, 150, "mininum timeout to make a new election");
DEFINE_int32(elect_timeout_max, 300, "maximum timeout to make a new election");
DEFINE_int64(session_expire_timeout, 6000000, "timeout for session expiration, 6 seconds in default");
DEFINE_int32(max_write_pending, 10000, "max write pending size of Put");
DEFINE_int32(max_commit_pending, 10000, "max commit pending size");
DEFINE_bool(ins_data_compress, true, "enable snappy compression on leveldb storage");
DEFINE_bool(ins_binlog_compress, true, "enable snappy compression on binlog");
DEFINE_int32(ins_gc_interval, 60, "binlog clean interval (seconds)");
DEFINE_int32(ins_max_throughput_in, -1, "max input throughput, MB");
DEFINE_int32(ins_max_throughput_out, -1, "max output throughput, MB");
DEFINE_int32(ins_data_block_size, 4, "for data, leveldb block_size, KB");
DEFINE_int32(ins_binlog_block_size, 4, "for binlog, leveldb block_size, KB");
DEFINE_int32(ins_data_write_buffer_size, 4, "for data, leveldb write_buffer_size, MB");
DEFINE_int32(ins_binlog_write_buffer_size, 4, "for binlog, leveldb write_buffer_size, MB");
DEFINE_int32(performance_interval, 1000, "milliseconds of the interval of performance counter ticktock");
DEFINE_int32(performance_buffer_size, 60, "size of the buffer to hold the history record of performance data");

//ins_cli only
DEFINE_string(ins_cmd, "", "the command of inc shell");
DEFINE_string(ins_key, "key", "");
DEFINE_string(ins_start_key, "", "start key of scan");
DEFINE_string(ins_end_key, "", "end key of scan (excluded)");
DEFINE_string(ins_value, "v123", "");
DEFINE_int64(ins_rm_binlog_index, 0, "end index of binlog clean operation");
DEFINE_string(ins_rm_binlog_server_id, "", "servier id of binlog clean operation");
DEFINE_int32(ins_watch_timeout, 120, "wath timeout(seconds)");
DEFINE_int32(ins_backup_watch_timeout, 115, "backup watch timeout(seconds)");
