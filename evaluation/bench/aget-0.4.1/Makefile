# EnderUNIX Aget Makefile
# http://www.enderunix.org/aget/

OBJS = main.o Aget.o Misc.o Head.o Signal.o Download.o Resume.o
CFLAGS = -g -W
LDFLAGS = -pthread
CC = gcc
STRIP = strip
CLANG = clang -emit-llvm


BREAKOBJ = "/home/ali/Desktop/Break/build33/Debug+Asserts/lib"
BREAKSRC = "/home/ali/DSLab/Break/"

.PHONY: all clean c break-yi break-lt




all: $(OBJS)
	$(CC) -o aget $(OBJS) $(LDFLAGS)

bc:
	$(CLANG) -g -c -o main.o main.c
	$(CLANG) -g -c -o Aget.o Aget.c
	$(CLANG) -g -c -o Misc.o Misc.c
	$(CLANG) -g -c -o Head.o Head.c
	$(CLANG) -g -c -o Signal.o Signal.c
	$(CLANG) -g -c -o Download.o Download.c
	$(CLANG) -g -c -o Resume.o Resume.c
	
break-yi: bc
	for i in $(OBJS); do opt -load $(BREAKOBJ)/Instrumentation.so -lock-instrument -o $$i < $$i; done
	clang++ -g -c -o YieldInjector.o $(BREAKSRC)/tools/YieldInjector/YieldInjector.cpp
	clang++ -emit-llvm -Wl,-plugin-opt=also-emit-llvm -g $(OBJS) YieldInjector.o -lpthread -o aget-yi

break-lt: bc
	for i in $(OBJS); do opt -load $(BREAKOBJ)/Instrumentation.so -lock-instrument -o $$i < $$i; done
	for i in $(OBJS); do opt -load $(BREAKOBJ)/Instrumentation.so -mem-instrument -o $$i < $$i; done
	clang++ -g -c -o LockTracer.o $(BREAKSRC)/tools/LockTracer/LockTracer.cpp
	clang++ -emit-llvm -Wl,-plugin-opt=also-emit-llvm -g $(OBJS) LockTracer.o -lpthread -o aget-lt

strip: $(all)
	$(STRIP) aget
	
install:
	cp -f aget /usr/local/bin/aget
	cp -f aget.1 /usr/share/man/man1/

clean: 
	rm -f aget aget-lt aget-yi *.txt *.bc *.o core.* *~

c:
	rm -f *core* *~ *log
