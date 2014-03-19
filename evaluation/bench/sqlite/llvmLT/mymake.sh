REAKOBJ="/home/ali/Desktop/Break/build33/Debug+Asserts/lib"
BREAKSRC="/home/ali/DSLab/Break/"

set -v 

make clean

make 

opt -load $BREAKOBJ/Instrumentation.so -lock-instrument -o os_unix.o < os_unix.o
opt -load $BREAKOBJ/Instrumentation.so -mem-instrument -o os_unix.o < os_unix.o


cd .libs/

opt -load $BREAKOBJ/Instrumentation.so -lock-instrument -o os_unix.o < os_unix.o
opt -load $BREAKOBJ/Instrumentation.so -mem-instrument -o os_unix.o < os_unix.o

cd ../


./libtool --mode=link --tag=CC clang  -emit-llvm -Wl,-plugin-opt=also-emit-llvm -g -O2 -DOS_UNIX=1 -DHAVE_USLEEP=1 -DHAVE_FDATASYNC=1 -I. -I../src -DNDEBUG  -I/usr/include/tcl8.5 -DTHREADSAFE=1 -DSQLITE_THREAD_OVERRIDE_LOCK=-1 -DSQLITE_OMIT_CURSOR -Wl,-plugin-opt=also-emit-llvm -o libsqlite3.la alter.lo analyze.lo attach.lo auth.lo btree.lo build.lo callback.lo complete.lo date.lo delete.lo expr.lo func.lo hash.lo insert.lo main.lo opcodes.lo os.lo os_unix.lo os_win.lo pager.lo parse.lo pragma.lo prepare.lo printf.lo random.lo select.lo table.lo tokenize.lo trigger.lo update.lo util.lo vacuum.lo vdbe.lo vdbeapi.lo vdbeaux.lo vdbefifo.lo vdbemem.lo where.lo utf.lo legacy.lo -lpthread \
                 -rpath /usr/local/lib -version-info "8:6:8"


clang++  -g -c -o LockTracer.o $BREAKSRC/tools/LockTracer/LockTracer.cpp

clang  -emit-llvm -Wl,-plugin-opt=also-emit-llvm -g -O2 -DOS_UNIX=1 -DHAVE_USLEEP=1 -DHAVE_FDATASYNC=1 -I. -I../src -DNDEBUG  -I/usr/include/tcl8.5 -DTHREADSAFE=1 -DSQLITE_THREAD_OVERRIDE_LOCK=-1 -DSQLITE_OMIT_CURSOR -Wl,-plugin-opt=also-emit-llvm -DHAVE_READLINE=1  -c -o shell.o ../src/shell.c

./libtool --mode=link --tag=CC clang++  -emit-llvm -Wl,-plugin-opt=also-emit-llvm -g -O2 -DOS_UNIX=1 -DHAVE_USLEEP=1 -DHAVE_FDATASYNC=1 -I. -I../src -DNDEBUG  -I/usr/include/tcl8.5 -DTHREADSAFE=1 -DSQLITE_THREAD_OVERRIDE_LOCK=-1 -DSQLITE_OMIT_CURSOR -Wl,-plugin-opt=also-emit-llvm -DHAVE_READLINE=1 -I/usr/include/readline -lpthread \
                -o sqlite3 shell.o LockTracer.o libsqlite3.la \
                -lreadline -lncurses


