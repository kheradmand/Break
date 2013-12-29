/*
 * LockTracer.cpp
 *
 *  Created on: Aug 21, 2013
 *      Author: ali
 */



#include <pthread.h>
#include <iostream>
#include <vector>
#include <map>
#include <queue>
#include <set>
#include <cstdio>
#include <assert.h>
#include <stdint.h>
#include <time.h>
#include <fstream>
#include <algorithm>
#include <sys/time.h>
using namespace std;


#define WAIT_TIME 1
//#define DEBUG

namespace LockTracer{

class Thread;

typedef int32_t Location;
typedef pthread_mutex_t* LockAddress;
typedef pair< Location , LockAddress> LockOperation;
typedef set<LockAddress> LockSet;

pthread_mutex_t tracerBigLock;

#ifdef DEBUG
ofstream log("trace.log");
#endif


map<Location, string> locationString;

typedef map<LockAddress, Thread*> LockThreadMap;
LockThreadMap lockThreadMap;
LockSet allLocksHeld;

pthread_t threads[1000];
int len;

int getIndex(const pthread_t &t){
	for (int i=0; i < len; i++){
		if (pthread_equal(threads[i], t) != 0)
			return i;
	}
	pthread_mutex_lock(&tracerBigLock);
	int retVal = len;
	threads[len++] = t;
	pthread_mutex_unlock(&tracerBigLock);
	return retVal;
}




void initialize(){
	//printf("tracer started\n");
	pthread_mutex_init(&tracerBigLock, NULL);
}

void beforeLock(pthread_mutex_t *m, Location loc){
#ifdef DEBUG
	//printf("%d: thread %d is going to lock %p\n", loc, getIndex(pthread_self()), m);
	//printf("ff\n");
	printf("+%d\n",sched_yield());
#else
	sched_yield();
#endif

}

void afterLock(pthread_mutex_t *m, Location loc){

}

void afterUnlock(pthread_mutex_t *m, Location loc){
#ifdef DEBUG
	printf("-%d\n",sched_yield());
#else
	sched_yield();
#endif
}

void finalize(){
#ifdef DEBUG
	printf("tracer ended\n");

	log.close();
#endif
}


}

/// Wrappers for linking purposes

void initialize(){
	//LockTracer::initialize();
}

void beforeLock(pthread_mutex_t *m, int32_t loc){

	LockTracer::beforeLock(m, loc);
	//================================================Idea #1=========================================
	//pthread_yield(); //wow! big effect
	//================================================================================================

}

void afterLock(pthread_mutex_t *m, int32_t loc){
	//LockTracer::afterLock(m, loc);
}

void beforeUnlock(pthread_mutex_t *m, int32_t loc){

}

void afterUnlock(pthread_mutex_t *m, int32_t loc){

	LockTracer::afterUnlock(m, loc);
}

void finalize(){
	LockTracer::finalize();
}
