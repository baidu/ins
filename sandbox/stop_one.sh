#!/bin/bash
n_th=$1
let port=$n_th+8867
pid=`ps aux | grep ins | grep $port | awk '{print $2}'`
kill -9 $pid


