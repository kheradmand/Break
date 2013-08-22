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
#include <cstdio>
#include <assert.h>
#include <stdint.h>
#include <fstream>
using namespace std;


namespace LockTracer{

typedef int32_t Location;
typedef pthread_mutex_t* LockAddress;
typedef pair< Location , LockAddress> LockOperation;

pthread_mutex_t tracerBigLock;

ofstream log("trace.log");

typedef map< LockAddress, int> AdjList;
map<LockAddress, AdjList> RAOG; //runtime address order graph

map<Location, string> locationString;

class Thread{
public:
	static Thread* self(){
		const pthread_t me = pthread_self();
		int len = threads.size();
		// it is OK if another thread push to vector, because that is definitely another thread.
		for (int i=0; i < len; i++){
			if (pthread_equal((threads[i]->pthread), me) != 0){
				return threads[i];
			}
		}

		//right now we lock at the beginning in order to keep everything simple
		//pthread_mutex_lock(&tracerLock);;
		threads.push_back(new Thread(me, threads.size()));
		//pthread_mutex_unlock(&tracerLock);

		return threads.back();
	}

	int getIndex(){
		assert(index != -1 && "thread index can not be -1\n");
		return index;
	}

	void lock(const LockOperation &lockAq){
		if (!locks.empty()){
			//add edge in runtime location order graph from all last locks to this lock
			LockAddress to = lockAq.second;
			for (typeof(locks.begin()) it = locks.begin(); it != locks.end(); it++){
				LockAddress from = locks.back().second;
				if (RAOG.find(from) == RAOG.end())
					RAOG[from] = AdjList();
				if (RAOG[from].find(to) == RAOG[from].end())
					RAOG[from][to] = 0;
				RAOG[from][to]++;
			}
		}
		locks.push_back(lockAq);
		log << "thread " << getIndex() << " locked " << lockAq.second << " at " << locationString[lockAq.first] << endl;

	}

	void unlock(const LockOperation &lockRel){
		locks.pop_back();
		log << "thread " << getIndex() << " unlocked " << lockRel.second << " at " << locationString[lockRel.first] << endl;
	}

private:
	static vector<Thread*> threads;
	const pthread_t pthread;
	int index;
	vector<LockOperation> locks;
	Thread(const pthread_t &p, int idx):pthread(p), index(idx){
	}
};



void printRuntimeAddressOrderGraph(){
	ofstream fout("runtime-address-order-graph.txt");
	fout << RAOG.size() << endl;
	for (typeof(RAOG.begin()) n = RAOG.begin(); n != RAOG.end(); n++){
		fout << n->first << " " << n->second.size() << endl;
		for (AdjList::iterator e = n->second.begin(); e != n->second.end(); e++){
			fout << "\t" << e->first << " " << e->second ;
		}
		fout << endl;
	}
	fout.close();
}

void logRuntimeAddressOrderGraph(){
	log << RAOG.size() << endl;
	for (typeof(RAOG.begin()) n = RAOG.begin(); n != RAOG.end(); n++){
		log << n->first << " " << n->second.size() << endl;
		for (AdjList::iterator e = n->second.begin(); e != n->second.end(); e++){
			log << "\t" << e->first << " " << e->second ;
		}
		log << endl;
	}
}

void readRuntimeAddressOrderGraph(){
	ifstream fin("runtime-address-order-graph.txt");

	int nodes;
	if (fin >> nodes){
		for (int i = 0; i < nodes; i++){
			void* temp;
			LockAddress n;
			int neibs;
			fin >> temp >> neibs;
			n = (LockAddress)temp;
			RAOG[n] = AdjList();
			for (int j = 0; j < neibs; j++){
				LockAddress m;
				int w;
				fin >> temp >> w;
				m = (LockAddress)temp;
				RAOG[n][m] = w;
			}
		}
	}

	fin.close();
	log << "initial runtime location order graph:\n";
	logRuntimeAddressOrderGraph();
}


void initLocationStringMap(){
	ifstream fin("location-index-map.txt");
	string str;
	while (fin >> str){
		Location loc;
		fin >> loc;
		locationString[loc] = str;
	}
	fin.close();
}

//class Graph{
//public:
//
//	class Node{
//	public:
//		Location value;
//
//
//	};
//};


vector<Thread*> Thread::threads = vector<Thread*>();


void initialize(){
	printf("tracer started\n");
	pthread_mutex_init(&tracerBigLock, NULL);
	initLocationStringMap();
	readRuntimeAddressOrderGraph();


}

void beforeLock(pthread_mutex_t *m, Location loc){
	printf("%d: thread %d is going to lock %p\n", loc, Thread::self()->getIndex(), m);
}

void afterLock(pthread_mutex_t *m, Location loc){
	printf("%d: thread %d locked %p\n", loc, Thread::self()->getIndex(), m);
	Thread::self()->lock(LockOperation(loc, m));
}

void afterUnlock(pthread_mutex_t *m, Location loc){
	printf("%d: thread %d released %p\n", loc, Thread::self()->getIndex(), m);
	Thread::self()->unlock(LockOperation(loc, m));
}

void finalize(){
	printf("tracer ended\n");

	log << "final run time location order graph:" << endl;
	logRuntimeAddressOrderGraph();

	printRuntimeAddressOrderGraph();
	log.close();
}


}

/// Wrappers for linking purposes

void initialize(){
	LockTracer::initialize();
}

void beforeLock(pthread_mutex_t *m, int32_t loc){
	pthread_mutex_lock(&LockTracer::tracerBigLock);
	LockTracer::beforeLock(m, loc);
	pthread_mutex_unlock(&LockTracer::tracerBigLock);
	//pthread_yield();
}

void afterLock(pthread_mutex_t *m, int32_t loc){
	pthread_mutex_lock(&LockTracer::tracerBigLock);
	LockTracer::afterLock(m, loc);
	pthread_mutex_unlock(&LockTracer::tracerBigLock);
}

void afterUnlock(pthread_mutex_t *m, int32_t loc){
	pthread_mutex_lock(&LockTracer::tracerBigLock);
	LockTracer::afterUnlock(m, loc);
	pthread_mutex_unlock(&LockTracer::tracerBigLock);
}

void finalize(){
	LockTracer::finalize();
}
