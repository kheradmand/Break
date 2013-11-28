/*
 * LockTracer.c
 *
 *  Created on: Aug 21, 2013
 *      Author: ali
 */



#include <pthread.h>

#include <assert.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>


#define WAIT_TIME 1



typedef int32_t Location;
typedef pthread_mutex_t* LockAddress;

pthread_mutex_t tracerBigLock;






void func1(){
	printf("tracer started\n");
	pthread_mutex_init(&tracerBigLock, NULL);
}

void func2(pthread_mutex_t *m, Location loc){
	//printf("%d: thread %d is going to lock %p\n", loc, getIndex(pthread_self()), m);
	//printf("ff\n");
	printf("+%d\n",sched_yield());
}

void func3(pthread_mutex_t *m, Location loc){

}

void func4(pthread_mutex_t *m, Location loc){

}

void func5(pthread_mutex_t *m, Location loc){
	printf("-%d\n",sched_yield());
}

void func6(){
	printf("tracer ended\n");

}




