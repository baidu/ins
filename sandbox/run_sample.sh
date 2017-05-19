#!/bin/bash
../output/bin/sample write --flagfile=./nexus.flag
if [ $? -eq 0 ]; then
	../output/bin/sample read --flagfile=./nexus.flag
fi

