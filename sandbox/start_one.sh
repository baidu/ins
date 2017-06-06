#!/bin/bash
n_th=$1
nohup ../output/bin/nexus --flagfile=nexus.flag --server_id=$n_th &>$n_th.log &
