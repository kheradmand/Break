#!/usr/bin/env bash

HERE=$(pwd)


function usage {
	echo "usage: $0 [start/stop/log] <executable name> [<number of parrallel runs>]";
	exit 1;
}

if [[ $# > 1 ]]; then
	case $1 in
		start)
		if [[ $# < 3 ]]; then
			usage;
		fi
		if [ -d $2_test_logs ]; then
			echo "another test is already running";
			exit 1;
		fi
		echo "starting the tests...";
		mkdir $2_test_logs;
		for t in `seq 1 $3`; do
			echo -n test $t: ;
			(for i in `seq 1 1000000`; do 
				echo $i > $2_test_logs/$t.log;
				./$2 >> $2_test_logs/$t.log;
				echo -1 > $2_test_logs/$t.log.old;
			done)&
			echo $! >> $2_test_logs/pid
			echo $!;
		done			
		;;
		stop)
		if [ ! -d $2_test_logs ]; then
			echo "no such test is running";
			exit 1;
		fi
		echo "stopping the tests...."
		for i in `cat $2_test_logs/pid`; do
			echo $i;
			kill $i;
		done
		ps aux | grep $2;
		rm -r $2_test_logs;
		;;
		log)
		if [ ! -d $2_test_logs ]; then
			echo "no such test is running";
			exit 1;
		fi
		for i in `ls $2_test_logs/*.log`; do
			echo $i:;
			cat  $i ;
			if [ `cat $i` = `cat $i.old` ]; then
				echo -n "looks like deadlock!"; 
			fi
			cat $i > $i.old;
			echo;
		done
		;;
	esac
else
	usage;
fi


