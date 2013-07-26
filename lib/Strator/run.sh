#/bin/bash

DEFAULT=~/LLVM-3.1/llvm/lib/Transforms/cord/Evaluation/pbzip2-0.9.4/pbzip2.bc

echo "Test program:"
read PROG

if [ -n $PROG ]; then
    PROG=$DEFAULT
fi

opt -mem2reg -scev-aa -tbaa -load `llvm-config --libdir`/LLVMStartor.so -strator < $PROG
