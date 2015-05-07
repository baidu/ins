#!/bin/bash
n_th=$1
let port=$n_th+8867
nohup ../output/bin/ins --flagfile=ins.flag --ins_port=$port &>$n_th.log &


