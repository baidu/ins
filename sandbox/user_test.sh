#!/bin/bash

./start_all.sh

sleep 1
./show_cluster.sh

sleep 2
../output/bin/ins_cli --ins_cmd=register --flagfile=nexus.flag --ins_key=zhangkai --ins_value=123456

sleep 1
COUNTER=0
while [ $COUNTER -lt 100 ];
do
	printf 'put %d value\nscan\nlogout\n' $COUNTER | nohup ../output/bin/ins_cli --ins_cmd=login --flagfile=nexus.flag --ins_key=zhangkai --ins_value=123456 &> out &
	let COUNTER=COUNTER+1
done

