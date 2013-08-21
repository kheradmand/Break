/*
 * LockTracer.cpp
 *
 *  Created on: Aug 21, 2013
 *      Author: ali
 */



#include <pthread.h>
#include <iostream>
#include <vector>
#include <cstdio>
using namespace std;


vector<pthread_t> threads;

int getIndex(const pthread_t &t){
	for (int i=0; i < (int)threads.size(); i++){
		if (pthread_equal(threads[i], t) != 0)
			return i;
	}
	threads.push_back(t);
	return threads.size()-1;
}




void initialize(){
	printf("tracer started\n");
	//cout << "tracer started" << endl;
}

void beforeLock(pthread_mutex_t *m){
	printf("thread %d is going to lock %p\n", getIndex(pthread_self()), m);
	//cout << "thread " << getIndex(pthread_self()) << " is going to lock " << m << endl;
}

void afterLock(pthread_mutex_t *m){
	printf("thread %d is locked %p\n", getIndex(pthread_self()), m);
	//cout << "thread " << getIndex(pthread_self()) << " locked " << m << endl;
}

void afterUnlock(pthread_mutex_t *m){
	printf("thread %d released %p\n", getIndex(pthread_self()), m);
	//cout << "thread " << getIndex(pthread_self()) << " released " << m << endl;
}

void finalize(){
	printf("tracer ended\n");
	//cout << "tracer ended" << endl;
}
