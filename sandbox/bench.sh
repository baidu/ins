#!/bin/bash
for ((i=1;i<=100;i++))
do
	./run_sample.sh &
done
wait

