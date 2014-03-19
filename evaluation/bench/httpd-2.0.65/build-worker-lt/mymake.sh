BREAKOBJ="/home/ali/Desktop/Break/build33/Debug+Asserts/lib"
BREAKSRC="/home/ali/DSLab/Break/"

set -v 

opt -load $BREAKOBJ/Instrumentation.so -lock-instrument -o httpd.bc < httpd.bc
opt -load $BREAKOBJ/Instrumentation.so -mem-instrument -o httpd.bc < httpd.bc

clang++ -g -c -o LT.o $BREAKSRC/tools/LockTracer/LockTracer.cpp

llc httpd.bc
clang++ -o httpd httpd.s -lrt -lm -lcrypt -lnsl -lpthread -ldl LT.o
