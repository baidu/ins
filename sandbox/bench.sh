#!/bin/bash
for ((i=1;i<=200;i++))
do
	./run_sample.sh &
done
wait

