#!/bin/bash
while true
do
	read -e -p "galaxy ins> " cmd arg1 arg2
	case $cmd in
	"show")
		sh ./show_cluster.sh
	;;

	"put")
		sh ./test_put.sh $arg1 $arg2
	;;

	"get")
		sh ./test_get.sh $arg1
	;;

	"getq")
		sh ./test_getq.sh $arg1
	;;

	"delete")
		sh ./test_del.sh $arg1
	;;
	"scan")
		sh ./test_scan.sh $arg1 $arg2
	;;
	"watch")
		sh ./test_watch.sh $arg1 &
	;;
	"quit")
		exit 0
	;;

	*)
		echo "  show [ show cluster ]"
		echo "  put (key) (value) [ update the data ] "
	        echo "  get (key) [read the data by key ]"	
		echo "  delete (key) [remove the data by key]"
		echo "  scan (start key) (end key, excluded)"
		echo "  watch (key)"
		echo "  enter quit to exit shell"
	;;

	esac
done

