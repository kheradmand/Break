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
		let all=0;
		let deadlocks=0;
		let cul=0;
		let culdead=0;
		for i in `ls $2_test_logs/*.log`; do
			echo $i:;
			cat  $i ;
			let all=all+1;
			let cul=cul+`cat $i`;
			if [ `cat $i` = `cat $i.old` ]; then
				echo -n "looks like deadlock!";
				let deadlocks=deadlocks+1;
				let culdead=culdead+`cat $i`; 
			fi
			cat $i > $i.old;
			echo;
		done
		if [[ $all > 0 ]]; then
			let avg=cul/all;
			echo deadlocks: $deadlocks / $all;
			echo average runs: $avg;
			if [[ $deadlocks > 0 ]]; then
				let avgdead=culdead/deadlocks;
				echo average run on deadlocks: $avgdead;
			fi 
		fi
		;;
	esac
else
	usage;
fi


