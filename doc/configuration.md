# Configuration Manual

中文配置手册请参考[这里](configuration_cn.md)  

**nexus** uses GFlags library to parse and hold command line parameters. For intergrated SDK, GFlags function `google::SetCommandLineOption("option", "value")` can be used to set options without using GFlags to parse command line.

Following are the configuration iterms in cluster node and client side.

## Cluster

| Options                        | Default    | Explanation                                                     |
| ------------------------------ | ---------- | --------------------------------------------------------------- |
| `cluster_members`              | `""`       | comma divided member list. e.g.: 127.0.0.1:8868,127.0.0.1:8869  |
| `server_id`                    | `1`        | offset of current node in member list                           |
| `ins_data_dir`                 | `"data"`   | path to store data                                              |
| `ins_binlog_dir`               | `"binlog"` | path to store raft log                                          |
| `max_cluster_size`             | `10`       | max size of cluster, must be bigger than member list size       |
| `log_rep_batch_max`            | `500`      | max number of raft log in a single log replication request      |
| `replication_retry_timespan`   | `2000`     | wait time before retrying a failed replication in ms            |
| `elect_timeout_min`            | `150`      | min time of an election timeout in ms                           |
| `elect_timeout_max`            | `300`      | max time of an election timeout in ms                           |
| `session_expire_timeout`       | `6000000`  | time to decide a session timeout in us                          |
| `max_write_pending`            | `10000`    | max size of write queue, overflow will lead to write denial     |
| `max_commit_pending`           | `10000`    | max size of commit queue, overflow will lead to request denial  |
| `ins_data_compress`            | `true`     | whether data will be compressed before written to leveldb       |
| `ins_binlog_compress`          | `true`     | whether raft log will be compressed before written to leveldb   |
| `ins_gc_interval`              | `60`       | interval of garbage collection task in second                   |
| `ins_max_throughput_in`        | `-1`       | max in throughput of one node in MB/s, -1 represents unlimited  |
| `ins_max_throughput_out`       | `-1`       | max out throughput of one node in MB/s, -1 represents unlimited |
| `ins_data_block_size`          | `4`        | block size of data leveldb in MB                                |
| `ins_binlog_block_size`        | `4`        | block size of raft log leveldb in MB                            |
| `ins_data_write_buffer_size`   | `4`        | write buffer size of data leveldb in MB                         |
| `ins_binlog_write_buffer_size` | `4`        | write buffer size of raft log leveldb in MB                     |
| `performance_interval`         | `1000`     | interval of rpc statistic updating in ms                        |
| `performance_buffer_size`      | `60`       | buffer size of rpc statistics                                   |
| `ins_trace_ratio`              | `0.001`    | ratio of sampling rpc calling log                               |
| `ins_log_file`                 | `stdout`   | path to save log, stdout represents using stdout                |
| `ins_log_size`                 | `1024`     | single file size limit of log in MB                             |
| `ins_log_total_size`           | `10240`    | overall file size limit of log in MB                            |

## SDK

SDK can use same flag file as node side, and options except following will be ignored. Following options can also be in node side flag file and will alse be ignored.

| Options                    | Default   | Explanation                                                                                 |
| -------------------------- | --------- | ------------------------------------------------------------------------------------------- |
| `cluster_members`          | `""`      | comma divided member list. e.g.: 127.0.0.1:8868,127.0.0.1:8869                              |
| `ins_log_file`             | `stdout`  | path to save log, stdout represents using stdout                                            |
| `ins_log_size`             | `1024`    | single file size limit of log in MB                                                         |
| `ins_log_total_size`       | `10240`   | overall file size limit of log in MB                                                        |
| `ins_watch_timeout`        | `120`     | interval of watch rpc in second                                                             |
| `ins_backup_watch_timeout` | `115`     | interval of backup watch in second                                                          |
| `ins_sdk_session_timeout`  | `6000000` | time to decide a session timeout in us, should not be bigger than `session_expired_timeout` |

## Client(Old Version)

Following options are used only in old version of client. The wrapper scripts are designed to shield all this options. And options in SDK is also useful.

| Options                    | Default   | Explanation                                                        |
| -------------------------- | --------- | ------------------------------------------------------------------ |
| `ins_cmd`                  | `""`      | command for client                                                 |
| `ins_key`                  | `"key"`   | key for get/put/lock/unlock/watch/register/login/logout operations |
| `ins_value`                | `"v123"`  | value for put/register/login/logout operations                     |
| `ins_start_key`            | `""`      | start key for scan operation                                       |
| `ins_end_key`              | `""`      | end key for scan operation                                         |
| `ins_rm_binlog_server_id`  | `""`      | server id to clean raft log(Deprecated)                            |
| `ins_rm_binlog_index`      | `0`       | index to clean raft log(Deprecated)                                |

## Client(New Version)

New designed client uses following options, and will not share these options with SDK and node. Also the client uses options in SDK.

| Options   | Default  | Explanation                                                         |
| --------- | -------- | ------------------------------------------------------------------- |
| `v`       | `false`  | enable verbose mode to output logs in all log level                 |
| `i`       | `false`  | enable interactive mode, data output format will be sightly changed |
| `h`       | `false`  | show help info                                                      |

