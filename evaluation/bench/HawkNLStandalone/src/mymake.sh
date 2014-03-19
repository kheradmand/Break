BREAKOBJ="/home/ali/Desktop/Break/build33/Debug+Asserts/lib"
BREAKSRC="/home/ali/DSLab/Break/"

make clean
rm -f deadlock3-LT deadlock3-YI


make CC="clang -emit-llvm"


for i in `ls *.o`; do
	echo $i;
	opt -load $BREAKOBJ/Instrumentation.so -lock-instrument -o $i < $i
done

clang++ -g -c -o YieldInjector.o $BREAKSRC/tools/YieldInjector/YieldInjector.cpp

clang++ -emit-llvm -lpthread -export-dynamic -g -o deadlock3-YI crc.o errorstr.o nl.o sock.o group.o loopback.o err.o thread.o mutex.o condition.o nltime.o deadlock3.o YieldInjector.o


rm YieldInjector.o

for i  in `ls *.o`; do
        echo $i;
        opt -load $BREAKOBJ/Instrumentation.so -mem-instrument -o $i < $i
done

clang++ -g -c -o LockTracer.o $BREAKSRC/tools/LockTracer/LockTracer.cpp

clang++ -emit-llvm -lpthread -export-dynamic -g -o deadlock3-LT crc.o errorstr.o nl.o sock.o group.o loopback.o err.o thread.o mutex.o condition.o nltime.o deadlock3.o LockTracer.o


rm LockTracer.o

