CC = gcc
CFLAGS = -Wall -g 
LD_FLAGS = ../build/.libs/libsqlite3.a -lpthread -export-dynamic
SRC = deadlock.c
OBJECTS = deadlock.o
TARGET = deadlock
LD = ld 
LYI = ../llvmYI/
LLT = ../llvmLT/
LYIpp = ../llvmYI++/
#KLEE_ARGS = --simplify-sym-indices --write-cvcs --write-cov --output-module --max-memory=1000 --disable-inlining --optimize --use-forked-stp --use-cex-cache --with-libc --allow-external-sym-calls --use-call-paths=false --fork-on-schedule=true --scheduler-preemption-bound=2 --with-file-model=release --init-env 


%o: %c
	$(CC) $(CFLAGS) -c $< -o $@

all: $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) $(LD_FLAGS) -o $(TARGET)
#	$(CC) $(CFLAGS) -c $(SRC) $(LD_FLAGS) -o $(TARGET)
#	$(LD) $(OBJECTS) -o $(TARGET) 

#run:
#	klee $(KLEE_ARGS) $(TARGET).bc

break-yic:
	$(LYI)/libtool --mode=link --tag=CC clang -emit-llvm -g -O2 -DOS_UNIX=1 -DHAVE_USLEEP=1 -DHAVE_FDATASYNC=1 -I. -I$(LYI) -DNDEBUG   -DTHREADSAFE=1 -DSQLITE_THREAD_OVERRIDE_LOCK=-1 -DSQLITE_OMIT_CURSOR -Wl,-plugin-opt=also-emit-llvm -DHAVE_READLINE=0  -lpthread -o deadlock deadlock.c $(LYI)/libsqlite3.la  $(LYI)/YieldInjector.o

break-yi:
	clang  -emit-llvm -Wl,-plugin-opt=also-emit-llvm -g -O2 -DOS_UNIX=1 -DHAVE_USLEEP=1 -DHAVE_FDATASYNC=1 -I. -I../src -DNDEBUG  -I/usr/include/tcl8.5 -DTHREADSAFE=1 -DSQLITE_THREAD_OVERRIDE_LOCK=-1 -DSQLITE_OMIT_CURSOR -Wl,-plugin-opt=also-emit-llvm -DHAVE_READLINE=1  -c -o deadlock.o deadlock.c
	$(LYIpp)/libtool --mode=link --tag=CC clang++  -emit-llvm -Wl,-plugin-opt=also-emit-llvm -g -O2 -DOS_UNIX=1 -DHAVE_USLEEP=1 -DHAVE_FDATASYNC=1 -I. -I../src -DNDEBUG  -I/usr/include/tcl8.5 -DTHREADSAFE=1 -DSQLITE_THREAD_OVERRIDE_LOCK=-1 -DSQLITE_OMIT_CURSOR -Wl,-plugin-opt=also-emit-llvm -DHAVE_READLINE=1 -I/usr/include/readline -lpthread \
                -o deadlock deadlock.o $(LYIpp)/YieldInjector.o $(LYIpp)/libsqlite3.la \
                -lreadline -lncurses


break-lt:
	clang  -emit-llvm -Wl,-plugin-opt=also-emit-llvm -g -O2 -DOS_UNIX=1 -DHAVE_USLEEP=1 -DHAVE_FDATASYNC=1 -I. -I../src -DNDEBUG  -I/usr/include/tcl8.5 -DTHREADSAFE=1 -DSQLITE_THREAD_OVERRIDE_LOCK=-1 -DSQLITE_OMIT_CURSOR -Wl,-plugin-opt=also-emit-llvm -DHAVE_READLINE=1  -c -o deadlock.o deadlock.c
	$(LLT)/libtool --mode=link --tag=CC clang++  -emit-llvm -Wl,-plugin-opt=also-emit-llvm -g -O2 -DOS_UNIX=1 -DHAVE_USLEEP=1 -DHAVE_FDATASYNC=1 -I. -I../src -DNDEBUG  -I/usr/include/tcl8.5 -DTHREADSAFE=1 -DSQLITE_THREAD_OVERRIDE_LOCK=-1 -DSQLITE_OMIT_CURSOR -Wl,-plugin-opt=also-emit-llvm -DHAVE_READLINE=1 -I/usr/include/readline -lpthread \
                -o deadlock deadlock.o $(LLT)/LockTracer.o $(LLT)/libsqlite3.la \
                -lreadline -lncurses

clean:
	rm -f $(TARGET)
	rm -f deadlock.o deadlock.bc deadlock.ll trace.log *.txt
