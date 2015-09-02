#!/bin/bash

./start_all.sh

sleep 1
./show_cluster.sh

sleep 2
../output/bin/ins_cli --ins_cmd=register --flagfile=ins.flag --ins_key=zhangkai --ins_value=123456
#../output/bin/ins_cli --ins_cmd=register --flagfile=ins.flag --ins_key=fxsjy --ins_value=123456

sleep 1
nohup ../output/bin/ins_cli --ins_cmd=login --flagfile=ins.flag --ins_key=zhangkai --ins_value=123456 <op1.txt &>out1.log &
nohup ../output/bin/ins_cli --ins_cmd=login --flagfile=ins.flag --ins_key=zhangkai --ins_value=123456 <op2.txt &>out2.log &

