#!/usr/bin/env bash

HERE=$(pwd)

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
		opt -load `llvm-config --libdir`/LLVMStrator.so -strator --global-strator=true < racer.bc
		;;
	esac
fi


