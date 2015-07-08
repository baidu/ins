#!/bin/bash
n_th=$1
pid=`ps aux | grep ins | grep -F "server_id=$n_th" | awk '{print $2}'`
kill -9 $pid


