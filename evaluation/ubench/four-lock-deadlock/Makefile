CILLY=/home/ali/relay-radar/cil/bin/cilly --merge --keepmerged -g
CC=clang -emit-llvm -g
BREAK_SRC=/home/ali/DSLab/Break/
.PHONY: racer normal

all: normal
	
normal: test.c
	gcc -g -o normal.out test.c -lpthread

relay: test.c
	$(CILLY) -c test.c
	$(CILLY) test.o -lpthread  -o relay
	echo "duppy -c test.c" > gcc-log.txt
	/home/ali/relay-radar/scripts/dump-with-stats.sh 


test.bc: test.c
	$(CC) -c test.c -o test.bc
	

racer: racer.bc

racer.bc: test.bc
	$(CC)  -Wl,-plugin-opt=also-emit-llvm test.bc -lpthread -o racer 
	llvm-dis racer.bc 
	llvm-dis test.bc


break-lt: test.bc
	$(BREAK_SRC)/tools/LockTracer/LinkTracer.sh test.bc
	
break-yi: test.bc
	$(BREAK_SRC)/tools/YieldInjector/LinkTracer.sh test.bc
	


.PHONY: clean

clean:
	rm -f *.o *.out *.bc *~ *.s *.ll *_comb.c *.txt *.log gcc-log.txt relay racer
	rm -rf ciltrees
