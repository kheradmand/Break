BUILDING THE TESTS:
Well, I did not have time to organize everything neatly and make a single makefile that can build and run all the tests. So, I will explain how to build each test individually.
First, you need to build the instrumentor's source code. Follow the instructions in readme.md inside the projects root. After that:

Tests inside ubench directory:
- edit BREAK_SRC variable in the Makefile, accordingly. 
- normal buld: 
	run make
	normal.out will be the target executable.
- SP: 
	run make break-yi
 	traced.out will be the tagete executable.
- CBD: 
	run make break-lt
	traced.out will be the traget executable.



Tests inside bench directory:
sqlite:
unfortunately most of the work should be done manually. 
- normal build:
	goto sqlite/build
	run ../configure
	run make 
	go to sqlite/deadlock_test
	run make
	deadlock will be the target executable
- SP:
	goto sqlite/llvmYI++
	run ../configure
	edit CC in libtool to "clang -v -emit-llvm"
	edit gcc in BCC and TCC in Makefile into "clang -emit-llvm -Wl,-plugin-opt=also-emit-llvm -g -O2"
	(in case it is otherwise) change a line in Makefile to "TCC += -DTHREADSAFE=1" and "LIBPTHREAD=-lpthread"
	run make
	edit BRAEKSRC and BREAKOBJ in mymake.sh accordingly
	(worst part) edit the libool invokation lines in mymake.sh according to your system specific and enviromental settings. (which you can obtain by refering to make's log)
	run mymake.sh
	goto sqlite/deadlock_test
	run make break-yi
	deadlock will be the target executable
- CBD:
	goto sqlite/llvmLT
	do the same as SP
	goto sqlite/deadlock_test
	run make break-lt
	deadlock will be the target executable


HawkNLStandalone:
- goto HawkNLStandalone/src
- type make
- edit BREAKSRC and BREAKPBJ in mymake.sh accordingly
- run mymake.sh
- normal:
	deadlock3 will be the target executable
- SP:
	deadlock3-YI will be the target executable
- CBD: 
	deadlock3-LT will be the target executable


pbzip2-1.1.6-ac:
- edit OPT and BREAK variables in Makefile accordingly
- normal:
	run make
	pbzip2 will be the target executable
- SP:
	run make break-yi
	break-yi will be the target executable
- CBD:
	run make break-lt 
	break-lt will be the target executable 

	  
	
aget-0.4.1:
- edit BREAKSRC and BREAKOBJ in Makefile accordingly 
- normal:
	run make
	aget will be the target executable
- SP: 
	run make break-yi 
	aget-yi will be the target executable
- CBD: 
	run make break-lt 
	aget-lt will be the target executable 



httpd-2.0.65:
- normal:
	goto httpd-2.0.65/build-worker-normal
	run (accordingly) ../configure --disable-shared --enable-static-support --enable-static-htpasswd --enable-static-htdigest --enable-static-rotatelogs --enable-static-logresolve --enable-static-htdbm --enable-static-ab --enable-static-checkgid --with-mpm=worker --prefix=/path/to/break/evaluation/bench/httpd-2.0.65/build-worker-normal/prefix
	run make
	run make install
	httpd will be the target executable
- SP:
	goto httpd-2.0.65/build-worker-yi
	run (accordingly) ../configure --disable-shared --enable-static-support --enable-static-htpasswd --enable-static-htdigest --enable-static-rotatelogs --enable-static-logresolve --enable-static-htdbm --enable-static-ab --enable-static-checkgid --with-mpm=worker --prefix=/path/to/break/evaluation/bench/httpd-2.0.65/build-worker-yi/prefix CC=clang -emit-llvm LDFLAGS=-Wl,-plugin-opt=also-emit-llvm RANLIB=ar --plugin /path/to/LLVMgold.so -s AR_FLAGS=--plugin path/to/LLVMgold.so -cru
	run make
	edit BREAKSRC and BREAKOBJ in mymake.sh accordingly
	run mymake.sh
	run make install
	httpd will be the target executable
	
- CBP:
	goto httpd-2.0.65/build-worker-lt
        run (accordingly) ../configure --disable-shared --enable-static-support --enable-static-htpasswd --enable-static-htdigest --enable-static-rotatelogs --enable-static-logresolve --enable-static-htdbm --enable-static-ab --enable-static-checkgid --with-mpm=worker --prefix=/path/to/break/evaluation/bench/httpd-2.0.65/build-worker-lt/prefix CC=clang -emit-llvm LDFLAGS=-Wl,-plugin-opt=also-emit-llvm RANLIB=ar --plugin /path/to/LLVMgold.so -s AR_FLAGS=--plugin path/to/LLVMgold.so -cru
        run make
        edit BREAKSRC and BREAKOBJ in mymake.sh accordingly
        run mymake.sh
        run make install
        httpd will be the target executable

- you can use Apache Benchmark (ab) tool (in prefix/bin) to establish simultaneous connections






RUNNING THE TESTS:
you can either run target executables manually or use the testing script in tools/scripts/test.sh. In the later case copy the script in the folder in which the executable resides. The script supports simultaneous instance executuions, (not precise) deadlock detection, and time measurement utilities. 


RUNNING THE TESTS WITH DEBUG INFO:
- for SP:
	uncomment #define DEBUG in tools/YieldInjector/YieldInjector.cpp
- for CBD:
	uncomment #define DEBUG in tools/LockTracer/LockTracer.cpp 


CBD WITH LP:
if you want to use CBD with LP instead of MP, rename tools/LockTracer/LockTracer-lp.cpp into LockTracer.cpp. If you want to measure overhead, you should disable memory instrumentation too. For this puporse edit Makefiles or mymake.sh files to remove the line containing "opt -load $BREAKOBJ/Instrumentation.so -mem-instrument ..."




