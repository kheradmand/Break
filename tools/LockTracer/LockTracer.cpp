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


#define WAIT_TIME 100
//#define DEBUG

static int getRandomWaitTime(int scale = 1){
	return WAIT_TIME+((rand()%(WAIT_TIME/2))*scale);
}


namespace LockTracer{

class Thread;

typedef int32_t Location;
typedef pthread_mutex_t* LockAddress;
typedef pair< Location , LockAddress> LockOperation;
typedef set<LockAddress> LockSet;

pthread_mutex_t tracerBigLock;
pthread_mutex_t pthreadOperationLock;

#ifdef DEBUG
ofstream log("trace.log");
#endif


map<Location, string> locationString;

typedef map<LockAddress, Thread*> LockThreadMap;
LockThreadMap lockThreadMap;
LockSet allLocksHeld;


class LockGraph{
public:
	class Node;
	typedef map<Node*, int> AdjacencyList;
	typedef set<Node*> NodeSet;
	class Node{
	private:
		AdjacencyList outEdges;
		AdjacencyList inEdges;
		LockAddress value;
		void addInEdge(Node* par, int w = 1){
			if (inEdges.find(par) == inEdges.end()){
				inEdges[par] = 0;
			}
			inEdges[par] += w;
		}
	public:
		Node(LockAddress val):value(val){
		}
		void addEdge(Node* nei, int w = 1){
			if (outEdges.find(nei) == outEdges.end()){
				outEdges[nei] = 0;
			}
			outEdges[nei] += w;
			nei->addInEdge(this, w);
		}

		LockAddress getValue() const{
			return value;
		}
		AdjacencyList getOutEdges() const{
			return outEdges;
		}
		AdjacencyList getInEdges() const{
			return inEdges;
		}
		int getEdgeWeight(Node* nei){
			if (outEdges.find(nei) == outEdges.end())
				return 0;
			return outEdges[nei];
		}
		NodeSet getOutNeibours(){
			NodeSet ret;
			for (AdjacencyList::iterator it = outEdges.begin(); it != outEdges.end(); it++){
				ret.insert(it->first);
			}
			return ret;
		}
		NodeSet getInNeibours(){
			NodeSet ret;
			for (AdjacencyList::iterator it = outEdges.begin(); it != outEdges.end(); it++){
				ret.insert(it->first);
			}
			return ret;
		}
		friend ostream& operator<<(ostream& out, const Node& node);
	};
private:
	map<LockAddress, Node*> lockNodeMap;
	string name;
	bool autosave;
public:
	LockGraph(const string& _name, bool autos = 0):name(_name), autosave(autos){
	}
	~LockGraph(){
		if (autosave)
			save();
	}

	string getName() const{
		return name;
	}

	Node* getNode(LockAddress value){
		if (lockNodeMap.find(value) == lockNodeMap.end())
			lockNodeMap[value] = new Node(value);
		return lockNodeMap[value];
	}

	void addEdge(LockAddress from, LockAddress to, int w = 1){
		getNode(from)->addEdge(getNode(to), w);
	}

	unsigned int size() const{
		return lockNodeMap.size();
	}

	NodeSet getNodeSet(){
		NodeSet ret;
		for (typeof(lockNodeMap.begin()) it = lockNodeMap.begin(); it != lockNodeMap.end(); it++)
			ret.insert(it->second);
		return ret;
	}

	friend ostream& operator<<(ostream& out, const LockGraph& graph);
	friend istream& operator>>(istream& out, LockGraph& graph);

	void load(){
		ifstream fin(("_"+name+".txt").c_str());
		fin >> *this;
		fin.close();

	}

	void save(){
		ofstream fout(("_"+name+".txt").c_str());
		fout << *this;
		fout.close();
	}

};


ostream& operator<<(ostream& out, const LockGraph::Node& node){
	out << node.getValue() << " " << node.outEdges.size() << endl;
	for (LockGraph::AdjacencyList::const_iterator it = node.outEdges.begin(); it != node.outEdges.end(); it++){
		out << "\t" << it->first->value << " " << it->second;
	}
	return out;
}
ostream& operator<<(ostream& out, const LockGraph& graph){
	out << graph.name << " " << graph.size() << endl;
	for (typeof(graph.lockNodeMap.begin()) n = graph.lockNodeMap.begin(); n != graph.lockNodeMap.end(); n++){
		out << *(n->second) << endl;
	}
	out << endl;
	return out;
}



istream& operator>>(istream& in, LockGraph& graph){
	graph.lockNodeMap.clear();
	int n;
	if (in >> graph.name){
		in >> n;
		for (int i = 0; i < n; i++){
			LockAddress adr;
			void* temp;
			in >> temp;
			adr = LockAddress(temp);
			LockGraph::Node* node = graph.getNode(adr);
			int m;
			in >> m;
			for (int j=0; j < m;j++){
				in >> temp;
				int w;
				in >> w;
				graph.addEdge(adr, LockAddress(temp), w);
			}
		}
	}
	return in;
}


LockGraph RAOG("runtime-address-order-graph", 1);

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
			for (typeof(locks.begin()) it = locks.begin(); it != locks.end(); it++)
				if (it->second > 0){
					LockAddress from = it->first;
#ifdef DEBUG
					log << "adding edge from " << from << " to " << to << endl;
#endif
					RAOG.addEdge(from, to);
				}
		}
		//locks.push_back(lockAq); locks order is troublesome, make it a set

		//locks[lockAq.second] = lockAq.first; there are recursive mutexes :(
		locks[lockAq.second]++;
		if (locks[lockAq.second] == 1)
			diffLocksHeldNum++;
		lockHistory.push_back(lockAq);
		//assert(allLocksHeld.find(lockAq.second) == allLocksHeld.end() && "thread locked a mutex which was locked!\n");
		lockThreadMap[lockAq.second] = this;
		allLocksHeld.insert(lockAq.second);
#ifdef DEBUG
		log << "+ thread " << getIndex() << " locked " << lockAq.second << " at " << locationString[lockAq.first] << endl;
#endif

	}

	void unlock(const LockOperation &lockRel){
		//locks.pop_back();
		//assert(locksHeld.find(lockRel.second) != locksHeld.end() && "thread unlocked a mutex which was not locked!\n");
		assert(locks[lockRel.second] > 0 && "thread unlocked a mutex which was not locked!\n");
		//locks.erase(lockRel.second);
		locks[lockRel.second]--;
		if (locks[lockRel.second] == 0)
			diffLocksHeldNum--;
		allLocksHeld.erase(lockRel.second);
		lockThreadMap.erase(lockRel.second);
#ifdef DEBUG
		log << "-   thread " << getIndex() << " unlocked " << lockRel.second << " at " << locationString[lockRel.first] << endl;
#endif
	}

	LockSet getLockset(){
		LockSet ret;
		for (typeof(locks.begin()) it = locks.begin(); it != locks.end(); it++){
			ret.insert(it->first);
		}
		return ret;
	}
	map<LockAddress, int> locks;
	vector<LockOperation> lockHistory;
	int diffLocksHeldNum;
private:
	static vector<Thread*> threads;
	const pthread_t pthread;
	int index;

	Thread(const pthread_t &p, int idx):pthread(p), index(idx), diffLocksHeldNum(0){
	}
};

#ifdef DEBUG
void logLockSet(const LockSet& lockSet){
	for (LockSet::iterator it = lockSet.begin(); it != lockSet.end(); it++){
		log << "\t" << *it << endl;
	}
}
#endif


//================================================Idea #3=========================================


map<LockGraph::Node*, int> component; //SCComponent number the node belongs to
LockGraph::NodeSet visited;
queue<LockGraph::Node*> topologicalOrder;
pthread_cond_t dummyCond;
int componentNum;
void topSort(LockGraph::Node* v){
	visited.insert(v);
	LockGraph::NodeSet neibs = v->getOutNeibours();
	for (LockGraph::NodeSet::iterator it = neibs.begin(); it != neibs.end(); it++){
		if (visited.find(*it) == visited.end()){
			topSort(*it);
		}
	}
	topologicalOrder.push(v);
}
void assignCompNumber(LockGraph::Node* v, int number){
#ifdef DEBUG
	log << "\t" << v->getValue();
#endif
	visited.insert(v);
	component[v] = number;
	LockGraph::NodeSet pars = v->getInNeibours();
	for (LockGraph::NodeSet::iterator it = pars.begin(); it != pars.end(); it++){
		if (visited.find(*it) == visited.end()){
			assignCompNumber(*it, number);
		}
	}
}
void findSCC(){
	pthread_cond_init(&dummyCond, NULL);

	LockGraph::NodeSet nodes = RAOG.getNodeSet();
	visited.clear();
	component.clear();
	componentNum = 0;

	for (LockGraph::NodeSet::iterator it = nodes.begin(); it != nodes.end(); it++){
		if (visited.find(*it) == visited.end()){
			topSort(*it);
		}
	}

	visited.clear();
#ifdef DEBUG
	log << "SCCs:\n";
#endif
	while (!topologicalOrder.empty()){
		LockGraph::Node* node = topologicalOrder.front();
		topologicalOrder.pop();
		if (visited.find(node) == visited.end()){
#ifdef DEBUG
			log << "Component #" << componentNum+1 << ":";
#endif
			assignCompNumber(node, ++componentNum);
#ifdef DEBUG
			log << endl;
#endif
		}
	}
}

//LockGraph depGraph("lock-dependecy-graph");



void checkSCC(Thread* thread, LockAddress lock){
	int comp;
	if (component.find(RAOG.getNode(lock)) == component.end()){
#ifdef DEBUG
		log << "WARNING: " << lock << " is new and does not have component number assigning 0 to it" << endl;
#endif
		comp = 0;
	}else
		comp = component[RAOG.getNode(lock)];
	int lastComp = 10000000; //inf
	if (thread->diffLocksHeldNum != 0){
		lastComp = component[RAOG.getNode(thread->lockHistory.back().second)];
	}
	if (lastComp <= comp){
#ifdef DEBUG
		log << "thread " << thread->getIndex() << " want to lock mutex in higher or same component, making it wait" << endl;
#endif
		timespec ts;
		timeval now;
		pthread_mutex_unlock(&tracerBigLock);
		usleep(getRandomWaitTime());
		pthread_mutex_lock(&tracerBigLock);
//		gettimeofday(&now, NULL);
//		ts.tv_sec = now.tv_sec + WAIT_TIME;
//		ts.tv_nsec = 0;
//		pthread_cond_timedwait(&dummyCond, &tracerBigLock, &ts);
	}


}

//================================================================================================


//================================================Idea #2=========================================
class Deadlock{
public:
	LockSet locks; //Interestingly order of locks does not matter
	void tryDeadlock(Thread* thread, LockAddress lock){
#ifdef DEBUG
		log << "at tryDeadlock" << endl;

		log << "all locks held:" << endl;
		logLockSet(allLocksHeld);
		log << "thread locks:" << endl;
		logLockSet(thread->getLockset());
		log << "deadlock locks:" << endl;
		logLockSet(locks);
#endif

		if (locks.find(lock) == locks.end())
			return;



		LockSet threadLocks = thread->getLockset();
		LockSet intersect;
		set_intersection(locks.begin(),locks.end(),threadLocks.begin(),threadLocks.end(),
				std::inserter(intersect,intersect.begin()));
#ifdef DEBUG
		log << "intersect:" << endl;
		logLockSet(intersect);
#endif

		if (intersect.empty())
			return;
#ifdef DEBUG
		log << "something to do here" << endl;
#endif
		//so thread has some locks of deadlock and wants a lock of it, we make it stop
		LockSet remaining;
		set_difference(locks.begin(), locks.end(), allLocksHeld.begin(), allLocksHeld.end(),
				std::inserter(remaining, remaining.begin()));
		if (remaining.empty()){
#ifdef DEBUG
			log << "all locks in deadlock are held, signaling all threads hoping for deadlock" << endl;
#endif
			pthread_cond_broadcast(&wait);
		}else{
#ifdef DEBUG
			log << "making thread wait" << endl;
#endif
			timespec ts;
			timeval now;
			gettimeofday(&now, NULL);
			ts.tv_sec = now.tv_sec + WAIT_TIME;
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

/// selects a deadlock to reveal
///TODO: this is just a temporary approach
Deadlock theDeadlock;
void selectDeadlock(){
	LockGraph::NodeSet nodes = RAOG.getNodeSet();
	for (LockGraph::NodeSet::iterator it = nodes.begin(); it != nodes.end(); it++){
		LockGraph::Node* node = *it;
		map<LockGraph::Node*, LockGraph::Node*> parent;
		queue<LockGraph::Node*> queue;
		queue.push(node);
		parent[node] = 0;
		while (!queue.empty()){
			LockGraph::Node* from = queue.front();
			queue.pop();
			LockGraph::NodeSet neibs = from->getOutNeibours();
			for (LockGraph::NodeSet::iterator nei = neibs.begin(); nei != neibs.end(); nei++){
				LockGraph::Node* to = *nei;
				if (parent.find(to) != parent.end()){
#ifdef DEBUG
					log << "found cycle: ";
#endif
					LockGraph::Node* temp = from;
					while (temp != to){
#ifdef DEBUG
						log << temp->getValue() << " - ";
#endif
						theDeadlock.locks.insert(temp->getValue());

						temp = parent[temp];
					}
#ifdef DEBUG
					log << to << endl;
#endif
					theDeadlock.locks.insert(to->getValue());
					return;
				}else{
					parent[to] = from;
					queue.push(to);
				}

			}

		}
	}
}

//================================================================================================


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


vector<Thread*> Thread::threads = vector<Thread*>();




void initialize(){
#ifdef DEBUG
	printf("tracer started\n");
#endif
	pthread_mutex_init(&tracerBigLock, NULL);
	pthread_mutex_init(&pthreadOperationLock, NULL);
	initLocationStringMap();
	RAOG.load();
#ifdef DEBUG
	log << "initial runtime location order graph:\n";
	log << RAOG << endl;
#endif
	findSCC();
	selectDeadlock();
}

void beforeLock(pthread_mutex_t *m, Location loc){
#ifdef DEBUG
	printf("%d: thread %d is going to lock %p\n", loc, Thread::self()->getIndex(), m);
	log << "thread " << Thread::self()->getIndex() << " going to lock " << m<< " at " << locationString[loc] << endl;
	//theDeadlock.tryDeadlock(Thread::self(), m);
#endif
	checkSCC(Thread::self(), m);
}

void afterLock(pthread_mutex_t *m, Location loc){
#ifdef DEBUG
	printf("%d: thread %d locked %p\n", loc, Thread::self()->getIndex(), m);
#endif
	Thread::self()->lock(LockOperation(loc, m));
}

void afterUnlock(pthread_mutex_t *m, Location loc){
#ifdef DEBUG
	printf("%d: thread %d released %p\n", loc, Thread::self()->getIndex(), m);
#endif
	Thread::self()->unlock(LockOperation(loc, m));
}

void finalize(){
#ifdef DEBUG
	printf("tracer ended\n");

	log << "final run time location order graph:" << endl;
	log << RAOG << endl;
#endif
	RAOG.save();
#ifdef DEBUG
	log.close();
#endif
}


}




/// Wrappers for linking purposes

void initialize(){
	srand(time(0));
	LockTracer::initialize();
}

void beforeLock(pthread_mutex_t *m, int32_t loc){
	pthread_mutex_lock(&LockTracer::tracerBigLock);
	LockTracer::beforeLock(m, loc);
	pthread_mutex_unlock(&LockTracer::tracerBigLock);
	//================================================Idea #1=========================================
	//pthread_yield(); //wow! big effect
	//================================================================================================

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


void beforeStore(void* address, int32_t loc){
	//printf("store %p\n", address);
	usleep(getRandomWaitTime());
}

void afterStore(void* address, int32_t loc){
	//printf("stored\n");
}

void beforeLoad(void* address, int32_t loc){
	//printf("load %p\n", address);
}

