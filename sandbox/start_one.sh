#!/bin/bash
n_th=$1
nohup ../output/bin/ins --flagfile=ins.flag --server_id=$n_th &>$n_th.log &
