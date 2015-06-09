#!/bin/bash
../output/bin/sample write --flagfile=./ins.flag
if [ $? -eq 0 ]; then
	../output/bin/sample read --flagfile=./ins.flag
fi

