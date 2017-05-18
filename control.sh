#!/bin/bash

# control.sh
#
# control script is used to manage the status of a single node
# by default, script assumes the package directory following a format:
#   +-- bin/
#    |- conf/
#    |- log/
#    |- data/
#    ...
# and will cd to bin/ for operation
# change $BIN_PATH, $CONF_PATH and $NEXUS_LOG_FILE to change this behaviour
#

# root path for a package
ROOT_PATH=$(dirname $(dirname $(readlink -f "$0")))
# bin path, subdir of root path by default
BIN_PATH=$ROOT_PATH/bin
# conf path, subdir of root path by default
CONF_PATH=$ROOT_PATH/conf

# set cluster member, e.g.: 127.0.0.1:8898,127.0.0.1:8899,127.0.0.1:8900
export NEXUS_CLUSTER_MEMBERS=""
# set type of host in cluster member, ip/hostname is allowed, use ip in above example
export NEXUS_HOST_TYPE="hostname"
# set dir to hold user defined data
export NEXUS_DATA_DIR="$ROOT_PATH/data/data"
# set dir to hold raft log data
export NEXUS_BINLOG_DIR="$ROOT_PATH/data/binlog"
# set path to save nexus log, use "" or "stdout" for printing log to stdout,
# otherwise it will create a symbolink at this path for latest log
export NEXUS_LOG_FILE="$ROOT_PATH/log/nexus.log"
# limit the max size of a single log file, left empty for unlimited
export NEXUS_LOG_SIZE=
# limit the max size of all log files, left empty for unlimited
export NEXUS_LOG_TOTAL_SIZE=

# definitions of return value
export AOS_OK=0
export AOS_CONFIG_ERROR=-1
export AOS_RUNTIME_ERROR=1

function build_env() {
    MY_NAME=""
    if [ "$NEXUS_HOST_TYPE" == "ip" ]; then
        MY_NAME="$(hostname -i)"
    else
        MY_NAME="$(hostname)"
    fi
    old_IFS=$IFS
    IFS=$','
    let n_th=1
    for node in $NEXUS_CLUSTER_MEMBERS; do
        hostname="$(echo $node | awk -F':' '{print $1}')"
        port="$(echo $node | awk -F':' '{print $2}')"
        if [ "$hostname" == "$MY_NAME" ]; then
            export NEXUS_SERVER_ID=$n_th
            export NEXUS_LISTEN_PORT=$port
            break
        fi
        let n_th=$n_th+1
    done
    IFS=$old_IFS
}

function generate_flag() {
    if [ "$NEXUS_CLUSTER_MEMBERS" == "" ]; then
        echo "no specified cluster members"
        return $AOS_CONFIG_ERROR
    fi
    if [ "$NEXUS_DATA_DIR" == "" ]; then
        echo "no specified data directory"
        return $AOS_CONFIG_ERROR
    fi
    if [ "$NEXUS_BINLOG_DIR" == "" ]; then
        echo "no specified binlog directory"
        return $AOS_CONFIG_ERROR
    fi
    echo "--cluster_members=$NEXUS_CLUSTER_MEMBERS" > $CONF_PATH/nexus.flag
    echo "--log_rep_batch_max=10" >> $CONF_PATH/nexus.flag
    echo "--ins_data_dir=$NEXUS_DATA_DIR" >> $CONF_PATH/nexus.flag
    echo "--ins_binlog_dir=$NEXUS_BINLOG_DIR" >> $CONF_PATH/nexus.flag
    if [ "$NEXUS_LOG_FILE" != "" ]; then
        echo "--ins_log_file=$NEXUS_LOG_FILE" >> $CONF_PATH/nexus.flag
    fi
    if [ "$NEXUS_LOG_SIZE" != "" ]; then
        echo "--ins_log_size=$NEXUS_LOG_SIZE" >> $CONF_PATH/nexus.flag
    fi
    if [ "$NEXUS_LOG_TOTAL_SIZE" != "" ]; then
        echo "--ins_log_total_size=$NEXUS_LOG_TOTAL_SIZE" >> $CONF_PATH/nexus.flag
    fi
    return $AOS_OK
}

function start() {
    healthcheck
    if [ $? -eq 0 ]; then
        return $AOS_OK
    fi
    generate_flag
    status=$?
    if [ $status -ne $AOS_OK ]; then
        return $status
    fi
    if [ "$NEXUS_SERVER_ID" == "" ]; then
        echo "calc server id error"
        return $AOS_CONFIG_ERROR
    fi
    mkdir -p $NEXUS_DATA_DIR
    if [ $? -ne 0 ]; then
        echo "unable to create data dir"
        return $AOS_CONFIG_ERROR
    fi
    mkdir -p $NEXUS_BINLOG_DIR
    if [ $? -ne 0 ]; then
        echo "unable to create binlog dir"
        return $AOS_CONFIG_ERROR
    fi
    if [ "$NEXUS_LOG_FILE" != "" ] && [ "$NEXUS_LOG_FILE" != "stdout" ]; then
        mkdir -p $(dirname $NEXUS_LOG_FILE)
        if [ $? -ne 0 ]; then
            echo "unable to create log dir"
            return $AOS_CONFIG_ERROR
        fi
    fi
    nohup $BIN_PATH/nexus --flagfile=$CONF_PATH/nexus.flag --server_id=$NEXUS_SERVER_ID </dev/null &>/dev/null & sleep 1
    sleep 5
    healthcheck
    if [ $? -ne 0 ]; then
        return $AOS_RUNTIME_ERROR
    fi
    return $AOS_OK
}

function stop() {
    if [ "$NEXUS_LISTEN_PORT" == "" ]; then
        echo "calc port error"
        return $AOS_CONFIG_ERROR
    fi
    nexus_pid=$(netstat -anop 2>/dev/null | grep ":$NEXUS_LISTEN_PORT " | grep LISTEN | awk '{print $7}' | awk -F'/' '{print $1}')
    if [ "$nexus_pid" == "" ]; then
        return $AOS_OK
    fi
    kill -9 $nexus_pid
    if [ $? -ne 0 ]; then
        return $AOS_RUNTIME_ERROR
    fi
    return $AOS_OK
}

function healthcheck() {
    if [ "$NEXUS_LISTEN_PORT" == "" ]; then
        echo "calc port error"
        return $AOS_CONFIG_ERROR
    fi
    nexus_pid=$(netstat -anop 2>/dev/null | grep ":$NEXUS_LISTEN_PORT " | grep LISTEN | awk '{print $7}' | awk -F'/' '{print $1}')
    if [ "$nexus_pid" == "" ]; then
        return $AOS_RUNTIME_ERROR
    fi
    nc -z 0.0.0.0 $NEXUS_LISTEN_PORT >/dev/null 2>&1
    if [ $? -ne 0 ]; then
        return $NEXUS_RUNTIME_ERROR
    fi
    kill -0 $nexus_pid
    if [ $? -ne 0 ]; then
        return $NEXUS_RUNTIME_ERROR
    fi
    return $AOS_OK
}

build_env
RETVAL=0
case "$1" in
    start)
        echo -e "[running]\tstart"
        start
        RETVAL=$?
        echo -e "return value:\t$RETVAL"
        ;;
    stop)
        echo -e "[running]\tstop"
        stop
        RETVAL=$?
        echo -e "return value:\t$RETVAL"
        ;;
    restart)
        echo -e "[running]\trestart"
        echo -e "[running]\tstop"
        stop
        echo -e "return value:\t$?"
        echo -e "[running]\tsleep and wait"
        sleep 60
        echo -e "[running]\tstart"
        start
        RETVAL=$?
        echo -e "return value:\t$RETVAL"
        ;;
    healthcheck)
        echo -e "[running]\thealthcheck"
        healthcheck
        RETVAL=$?
        echo -e "return value:\t$RETVAL"
        ;;
    status)
        echo -e "[running]\tstatus"
        healthcheck
        RETVAL=$?
        echo -e "return value:\t$RETVAL"
        ;;
    *)
        echo $"Usage: $0 {start|stop|restart|healthcheck|status}"
        REVAL=$AOS_CONFIG_ERROR
        ;;
esac
exit $RETVAL

