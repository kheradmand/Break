BREAKOBJ="/home/ali/Desktop/Break/build33/Debug+Asserts/lib"
BREAKSRC="/home/ali/DSLab/Break/"

set -v 

opt -load $BREAKOBJ/Instrumentation.so -lock-instrument -o httpd.bc < httpd.bc

clang++ -g -c -o YI.o $BREAKSRC/tools/YieldInjector/YieldInjector.cpp

llc httpd.bc
clang++ -o httpd httpd.s -lrt -lm -lcrypt -lnsl -lpthread -ldl YI.o
