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


namespace LockTracer{

class Thread;

typedef int32_t Location;
typedef pthread_mutex_t* LockAddress;
typedef pair< Location , LockAddress> LockOperation;
typedef set<LockAddress> LockSet;

pthread_mutex_t tracerBigLock;
pthread_mutex_t pthreadOperationLock;

ofstream log("trace.log");

typedef map< LockAddress, int> AdjList;
map<LockAddress, AdjList> RAOG; //runtime address order graph

map<Location, string> locationString;

typedef map<LockAddress, Thread*> LockThreadMap;
LockThreadMap lockThreadMap;
LockSet locksHeld;

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
				LockAddress from = it->second;
				if (RAOG.find(from) == RAOG.end())
					RAOG[from] = AdjList();
				if (RAOG[from].find(to) == RAOG[from].end())
					RAOG[from][to] = 0;
				RAOG[from][to]++;
			}
		}
		locks.push_back(lockAq);
		assert(locksHeld.find(lockAq.second) == locksHeld.end() && "thread locked a mutex which was locked!\n");
		lockThreadMap[lockAq.second] = this;
		locksHeld.insert(lockAq.second);
		log << "+ thread " << getIndex() << " locked " << lockAq.second << " at " << locationString[lockAq.first] << endl;

	}

	void unlock(const LockOperation &lockRel){
		locks.pop_back();
		assert(locksHeld.find(lockRel.second) != locksHeld.end() && "thread unlocked a mutex which was not locked!\n");
		locksHeld.erase(lockRel.second);
		lockThreadMap.erase(lockRel.second);
		log << "-   thread " << getIndex() << " unlocked " << lockRel.second << " at " << locationString[lockRel.first] << endl;
	}

	LockSet getLockset(){
		LockSet ret;
		for (typeof(locks.begin()) it = locks.begin(); it != locks.end(); it++){
			ret.insert(it->second);
		}
		return ret;
	}

private:
	static vector<Thread*> threads;
	const pthread_t pthread;
	int index;
	vector<LockOperation> locks;
	Thread(const pthread_t &p, int idx):pthread(p), index(idx){
	}
};


void logLockSet(const LockSet& lockSet){
	for (LockSet::iterator it = lockSet.begin(); it != lockSet.end(); it++){
		log << "\t" << *it << endl;
	}
}

class Deadlock{
public:
	LockSet locks; //Interestingly order of locks does not matter
	void tryDeadlock(Thread* thread, LockAddress lock){
		log << "at tryDeadlock" << endl;

		log << "all locks held:" << endl;
		logLockSet(locksHeld);
		log << "thread locks:" << endl;
		logLockSet(thread->getLockset());
		log << "deadlock locks:" << endl;
		logLockSet(locks);

		if (locks.find(lock) == locks.end())
			return;
		LockSet threadLocks = thread->getLockset();
		LockSet intersect;
		set_intersection(locks.begin(),locks.end(),threadLocks.begin(),threadLocks.end(),
				std::inserter(intersect,intersect.begin()));

		log << "intesect:" << endl;
		logLockSet(intersect);

		if (intersect.empty())
			return;
		log << "something to do here" << endl;
		//so thread has some locks of deadlock and wants a lock of it, we make it stop
		LockSet remaining;
		set_difference(locks.begin(), locks.end(), locksHeld.begin(), locksHeld.end(),
				std::inserter(remaining, remaining.begin()));
		if (remaining.empty()){
			log << "all locks in deadlock are held, signaling all threads hoping for deadlock" << endl;
			pthread_cond_broadcast(&wait);
		}else{
			log << "making thread wait" << endl;
			timespec ts;
			timeval now;
			gettimeofday(&now, NULL);
			ts.tv_sec = now.tv_sec + 1;
			ts.tv_nsec = 0;
			pthread_cond_timedwait(&wait, &tracerBigLock, &ts);
		}
	}
	Deadlock(){
		pthread_cond_init(&wait, NULL);
	}
private:
	pthread_cond_t wait;
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


/// selects a deadlock to reveal
///TODO: this is just a temperary approach
Deadlock theDeadlock;
void selectDeadlock(){

	for (typeof(RAOG.begin()) it = RAOG.begin(); it != RAOG.end(); it++){
		LockAddress node = it->first;
		map<LockAddress, LockAddress> parent;
		queue<LockAddress> queue;
		queue.push(node);
		parent[node] = 0;
		while (!queue.empty()){
			LockAddress from = queue.front();
			queue.pop();
			for (AdjList::iterator nei = RAOG[from].begin(); nei != RAOG[from].end(); nei++){
				LockAddress to = nei->first;
				if (parent.find(to) != parent.end()){
					log << "found cycle: ";
					LockAddress temp = from;
					while (temp != to){
						log << temp << " - ";
						theDeadlock.locks.insert(temp);
						temp = parent[temp];
					}
					log << to << endl;
					theDeadlock.locks.insert(to);
					return;
				}else{
					parent[to] = from;
					queue.push(to);
				}

			}

		}
	}
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
	pthread_mutex_init(&pthreadOperationLock, NULL);
	initLocationStringMap();
	readRuntimeAddressOrderGraph();
	selectDeadlock();
}

void beforeLock(pthread_mutex_t *m, Location loc){
	printf("%d: thread %d is going to lock %p\n", loc, Thread::self()->getIndex(), m);
	log << "thread " << Thread::self()->getIndex() << " going to lock " << m<< " at " << locationString[loc] << endl;
	theDeadlock.tryDeadlock(Thread::self(), m);
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

	//pthread_yield(); //wow! big effect


}

void afterLock(pthread_mutex_t *m, int32_t loc){
	pthread_mutex_lock(&LockTracer::tracerBigLock);
	LockTracer::afterLock(m, loc);
	pthread_mutex_unlock(&LockTracer::tracerBigLock);

}

void beforeUnlock(pthread_mutex_t *m, int32_t loc){
	pthread_mutex_lock(&LockTracer::tracerBigLock);
}

void afterUnlock(pthread_mutex_t *m, int32_t loc){

	LockTracer::afterUnlock(m, loc);
	pthread_mutex_unlock(&LockTracer::tracerBigLock);
}

void finalize(){
	LockTracer::finalize();
}
