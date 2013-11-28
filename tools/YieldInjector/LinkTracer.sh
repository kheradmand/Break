#!/usr/bin/env bash

BREAKSRC=~/DSLab/Break
TRACER=$BREAKSRC/tools/YieldInjector
BREAKOBJ=~/Desktop/Break/build33

if [[ $# > 0 ]]; then
	echo "Instumenting program..."
	opt -load $BREAKOBJ/Debug+Asserts/lib/Instrumentation.so -lock-instrument -o instrumented.bc < $1
	echo "Compiling Tracer..."
	clang++ -c -emit-llvm -o YieldInjector.bc $TRACER/YieldInjector.cpp 
	echo "Linking instrumeted code with Injector..."
	clang++ -g -emit-llvm -Wl,-plugin-opt=also-emit-llvm -o traced.out -lpthread  YieldInjector.bc instrumented.bc
	echo "Generating ll files..."
	llvm-dis instrumented.bc
	llvm-dis YieldInjector.bc
	llvm-dis traced.out.bc
	echo "DONE"
	echo "run traced.out"
fi


