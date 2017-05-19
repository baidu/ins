#!/bin/bash

host_name=`hostname`
echo "--cluster_members=$host_name:8868,$host_name:8869,$host_name:8870,$host_name:8871,$host_name:8872" >nexus.flag

nohup ../output/bin/nexus --flagfile=nexus.flag --server_id=1 &>1.log &
nohup ../output/bin/nexus --flagfile=nexus.flag --server_id=2 &>2.log &
nohup ../output/bin/nexus --flagfile=nexus.flag --server_id=3 &>3.log &
nohup ../output/bin/nexus --flagfile=nexus.flag --server_id=4 &>4.log &
nohup ../output/bin/nexus --flagfile=nexus.flag --server_id=5 &>5.log &




