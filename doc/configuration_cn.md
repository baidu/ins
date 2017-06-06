# 配置信息

For English version of this manual, please refer to [this](configuration.md)

**nexus**使用GFlags来处理命令行参数。对于SDK的使用者，如果不希望使用GFlags来处理命令行参数，可以通过使用`google::SetCommandLineOption("option", "value")`来修改下面提到的配置项。

下面列出集群和客户端使用的配置项。

## 集群配置

| 配置项                         | 默认值     | 作用                                                    |
| ------------------------------ | ---------- | ------------------------------------------------------- |
| `cluster_members`              | `""`       | 逗号分隔的集群成员，如example.com:8000,example.com:9000 |
| `server_id`                    | `1`        | 当前节点在集群成员中的偏移量                            |
| `ins_data_dir`                 | `"data"`   | 数据存放路径                                            |
| `ins_binlog_dir`               | `"binlog"` | 同步的log存放路径                                       |
| `max_cluster_size`             | `10`       | nexus集群最大节点数量                                   |
| `log_rep_batch_max`            | `500`      | 批量日志同步时单次同步最大值                            |
| `replication_retry_timespan`   | `2000`     | 日志同步失败后重试等待时间                              |
| `elect_timeout_min`            | `150`      | 选举超时时间的最小值，单位ms                            |
| `elect_timeout_max`            | `300`      | 选举超时时间的最大值，单位ms                            |
| `session_expire_timeout`       | `6000000`  | 客户端session超时时间，单位us                           |
| `max_write_pending`            | `10000`    | 写操作队列最大长度，超出会拒绝写请求                    |
| `max_commit_pending`           | `10000`    | commit队列最大长度，超出会拒绝日志同步                  |
| `ins_data_compress`            | `true`     | 数据写入leveldb时是否压缩                               |
| `ins_binlog_compress`          | `true`     | 同步的log写入leveldb时是否压缩                          |
| `ins_gc_interval`              | `60`       | 垃圾清零时间间隔，单位s                                 |
| `ins_max_throughput_in`        | `-1`       | nexus集群最大入带宽限制，单位MB/s，-1表示无限制         |
| `ins_max_throughput_out`       | `-1`       | nexus集群最大出带宽限制，单位MB/s，-1表示无限制         |
| `ins_data_block_size`          | `4`        | 数据存储leveldb块大小，单位MB                           |
| `ins_binlog_block_size`        | `4`        | 同步的log存储leveldb块大小，单位MB                      |
| `ins_data_write_buffer_size`   | `4`        | 数据存储leveldb写缓冲区大小，单位MB                     |
| `ins_binlog_write_buffer_size` | `4`        | 同步的log存储leveldb写缓冲区大小，单位MB                |
| `performance_interval`         | `1000`     | rpc数据统计单位时间，单位ms                             |
| `performance_buffer_size`      | `60`       | rpc数据统计缓冲区大小                                   |
| `ins_trace_ratio`              | `0.001`    | rpc调用时输出调用者地址到日志到概率                     |
| `ins_log_file`                 | `stdout`   | 日志文件位置，如为stdout则输出到标准输出                |
| `ins_log_size`                 | `1024`     | 日志文件单个文件大小限制，单位MB                        |
| `ins_log_total_size`           | `10240`    | 日志文件总大小限制，单位MB                              |

## SDK配置

SDK可以使用和集群相同的配置文件，多余的选项则会被忽略。集群端也可以设置下面的选项，同样也会被忽略。

| 配置项                     | 默认值    | 作用                                                                 |
| -------------------------- | --------- | -------------------------------------------------------------------- |
| `cluster_members`          | `""`      | 逗号分隔的集群成员，如example.com:8000,example.com:9000              |
| `ins_log_file`             | `stdout`  | 日志文件位置，如为stdout则输出到标准输出                             |
| `ins_log_size`             | `1024`    | 日志文件单个文件大小限制，单位MB                                     |
| `ins_log_total_size`       | `10240`   | 日志文件总大小限制，单位MB                                           |
| `ins_watch_timeout`        | `120`     | watch操作单次rpc超时时间，单位s                                      |
| `ins_backup_watch_timeout` | `115`     | 备份watch操作单词rpc超时时间，单位s                                  |
| `ins_sdk_session_timeout`  | `6000000` | sdk与集群的session超时时间，不应大于`session_expire_timeout`，单位us |

## 客户端（旧版）

下面的参数在旧版客户端中生效，为方便用户使用，使用脚本来封装了这些参数。

| 配置项                     | 默认值    | 作用                                            |
| -------------------------- | --------- | ----------------------------------------------- |
| `ins_cmd`                  | `""`      | 指定客户端的操作                                |
| `ins_key`                  | `"key"`   | 指定key用于get/lock/unlock/login/logout等操作   |
| `ins_value`                | `"v123"`  | 指定value用于get/lock/unlock/login/logout等操作 |
| `ins_start_key`            | `""`      | 指定start key用于scan操作                       |
| `ins_end_key`              | `""`      | 指定end key用于scan操作                         |
| `ins_rm_binlog_server_id`  | `""`      | 指定server id用于清理同步的log                  |
| `ins_rm_binlog_index`      | `0`       | 清理指定index前的所有同步的log                  |

## 客户端（新版）

新版的客户端使用的参数不与集群和SDK公用。除下面参数外还会使用SDK的参数。

| 配置项   | 默认值   | 作用                                                   |
| -------- | -------- | ------------------------------------------------------ |
| `v`      | `false`  | 设置后会输出所有级别的log，默认值输出warning及以上级别 |
| `i`      | `false`  | 交互模式，打开后部分操作的输出格式会有所不同           |
| `h`      | `false`  | 输出帮助信息                                           |

