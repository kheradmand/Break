#!/usr/bin/env bash

HERE=$(pwd)
BREAK=/home/ali/Desktop/Break/build33/Debug+Asserts/lib/
LLVM=/home/ali/LLVM-3.3/build/Debug+Asserts/
FLAGS=
if [[ $# > 0 ]]; then
	case $1 in
		relay)
		echo "Running RELAY"
		cd ~/relay-radar/
		./relay_single.sh $HERE/ciltrees > $HERE/relay.serv
		cd $HERE			
		;;
		racer)
		echo "Running RACER"
		$LLVM/bin/opt -load $BREAK/InterpAA.so -load $BREAK/Strator.so -anders-aa  -strator --global-strator=true --use-aa=true --print-lock-unlock=false --debug-alias=false < racer-m2r.ll
		;;
	esac
fi


