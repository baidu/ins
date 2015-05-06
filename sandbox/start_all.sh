#!/bin/bash

host_name=`hostname`
echo "--cluster_members=$host_name:8868,$host_name:8869,$host_name:8870,$host_name:8871,$host_name:8872" >ins.flag

nohup ../output/bin/ins --flagfile=ins.flag --ins_port=8868 &>1.log &
nohup ../output/bin/ins --flagfile=ins.flag --ins_port=8869 &>2.log &
nohup ../output/bin/ins --flagfile=ins.flag --ins_port=8870 &>3.log &
nohup ../output/bin/ins --flagfile=ins.flag --ins_port=8871 &>4.log &
nohup ../output/bin/ins --flagfile=ins.flag --ins_port=8872 &>5.log &




