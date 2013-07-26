/*
 * File: Strator.cpp
 *   
 * Author: kasikci
 * A static race detector based on LLVM
 */


#include "Version.h"

#include <iostream>
#include <sstream>
#include <iomanip>

#if LLVM_VERSION_CODE < LLVM_VERSION(3, 3)
#include "llvm/Instruction.h"
#include "llvm/Module.h"
#include "llvm/Constants.h"
#include "llvm/ValueSymbolTable.h"
#include "llvm/Operator.h"

#include "llvm/Support/circular_raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/CFG.h"
#include "llvm/PassManager.h"

#include "llvm/Assembly/Writer.h"

#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/DebugInfo.h"
#include "llvm/Analysis/AliasAnalysis.h"
#else
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/ValueSymbolTable.h"
#include "llvm/IR/Operator.h"

#include "llvm/Support/circular_raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/CFG.h"
#include "llvm/PassManager.h"

#include "llvm/Assembly/Writer.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/DebugInfo.h"
#include "llvm/Analysis/AliasAnalysis.h"
#endif

#include "Strator/Strator.h"

/// Uncomment this for verbose debugging
//#define DETAILED_DEBUG

using namespace llvm;
using namespace std;

namespace {
cl::opt<unsigned int>
RaceDetectorLevel("strator-level",
		cl::desc("Race detection precision level (1-3) (default=1)"),
		cl::init(1));

cl::opt<bool>
PrintLockUnlock("print-lock-unlock",
		cl::desc("Print debugging information regarding lock/unlock operations (default=false)"),
		cl::init(false));

cl::opt<bool>
PrintPrevPasses("print-prev-passes",
		cl::desc("This will print previous analysis results (default=false)"),
		cl::init(false));

cl::opt<bool>
ReportUnprocessed("report-unproc",
		cl::desc("This option will print the functions that are not processed after the end of race detection (default=false)"),
		cl::init(false));

cl::opt<bool>
DebugAliasAnalysis("debug-alias",
		cl::desc("Print some debug information regarding alias analysis (default=false)"),
		cl::init(false));

cl::opt<bool>
UseAliasAnalysis("use-aa",
		cl::desc("Use the results of alias analysis in race detection (default=true)"),
		cl::init(true));

cl::opt<bool>
UseLocalValueInfo("use-local-value-info",
		cl::desc("Use the results of local value identification information that relies on alias analysis"),
		cl::init(true));

cl::opt<bool>
DirectedRaceDetection("global-strator",
		cl::desc("Perform race detection on the entire bitocde, considering all possible entry points (default=false)"),
		cl::init(true));

cl::opt<bool>
DebugBinary("debug-binary",
		cl::desc("This is to turn on reporting of debug information (default=true)"),
		cl::init(true));

cl::opt<bool>
ReportLLVMInstruction("report-inst",
		cl::desc("Print corresponding LLVM instruction in race report (default=false)"),
		cl::init(false));

cl::opt<unsigned>
UpwardPropagationLevel("prop-level",
		cl::desc("This is to propagate multithreaded context information."),
		cl::init(0));
}

void Strator::getAnalysisUsage(AnalysisUsage& au) const {
//	if(UseLocalValueInfo || DebugAliasAnalysis)
//		au.addRequired<LocalIdentifier>();
	if (UseAliasAnalysis || DebugAliasAnalysis)
		au.addRequired<AliasAnalysis>();
	/// We employ multi threaded context finding during race detection rather than using a different pass
	// au.addRequired<MultithreadFinder>();
	//au.addRequired<UseDefBuilder>();
	au.addRequired<CallGraph>();
	au.setPreservesAll();
}

bool Strator::runOnModule(Module &m) {
	errs() << "Strator started!\n";
	/// Get previous analysis results
	// multithreadedFunctionMap = &(getAnalysis<MultithreadFinder>().multithreadedFunctionMap);
//	if(UseLocalValueInfo)
//		localValueInfo = &(getAnalysis<LocalIdentifier>().localValueInfo);
	if(UseAliasAnalysis || DebugAliasAnalysis)
		aa = &getAnalysis<AliasAnalysis>();
		//aa = getAnalysis<LocalIdentifier>().aa;
	//useDefMap = &(getAnalysis<UseDefBuilder>().useDefMap);
	CallGraph& callGraph = getAnalysis<CallGraph>();

	GEPFactory = new GEPValueFactory(&m);

	if(PrintPrevPasses){
		printLocalModificationInfo();
	}

	CallGraphNode* externalNode = callGraph.getExternalCallingNode();

	if (DirectedRaceDetection){
		/// This is the first and default race detection strategy
		/// that only tracks a main function (like the
		/// unit test case). We call this directed race detection
		/// There is no thread pool in this case but a single worker
		threadPoolSize = 0;

		/// We use this simply to mark all the external entry point functions as initially not analyzed
		CallGraphNode::iterator it;
		for(it = externalNode->begin(); it != externalNode->end(); ++it){
			Function* f = 0;
			if((f = it->second->getFunction()))
				if(f->size() > 0){
					functionMap[f->getName().str()] = false;
					++functionCount;
				}
		}

		// cerr << "Total entry point count: " << functionCount << endl;

		CallGraphNode* root = callGraph.getRoot();
		assert(root->size() && "Size of the call graph root is 0! Something is wrong");
		cerr << "Root calls: " << root->size() << " functions"<< endl;

		if(!root->getFunction()){
			cerr << "The root represents an external node" << endl;
			assert(false && "You need to switch to global race detection!");
		}
		// cerr << root->getFunction()->getName().str() << endl;
		/// Initialize all functions to not-multithreaded
		workers.push_back(new StratorWorker(this));
		for(it = root->begin(); it != root->end(); ++it){
			Function* f = 0;
			if((f = it->second->getFunction())){
				if(f->size() > 0){
					workers[0]->multithreadedFunctionMap[it->second->getFunction()->getName().str()] = false;
				}
			}
		}
		Strator::StratorWorker::LockSet* lockSet = new Strator::StratorWorker::LockSet();
		workers[0]->traverseFunction(*(root->getFunction()), *lockSet);
	} else {
		/// This is the second variant of race detection. Here all the external
		/// functions with definitions are considered as root and race detection
		/// is performed over all their childeren. this variant is called global
		/// race detection
		pthread_t tids[50];
		threadPoolSize = 50;

		if(externalNode){
			/// TODO: We should investigate the following: Some static functions (thus
			/// having internal linkage) are considered as external nodes as they are
			/// used as parameters to some functions (like pthread_create). We should
			/// understand if it is necessary to add such functions as external nodes.
			CallGraphNode::iterator it;
			for(it = externalNode->begin(); it != externalNode->end(); ++it){
				Function* f = 0;
				if((f = it->second->getFunction()))
					if(f->size() > 0){
						cerr << "adding function \"" << it->second->getFunction()->getName().str() << "\" to task list" << endl;
						tasks.push_back(f);
						++functionCount;
					}
			}
			/// Create the thread pool
			threadPoolSize = (threadPoolSize < functionCount) ? threadPoolSize : functionCount;
			/// Create as many workers as the pool size
			// threadPoolSize = 1;
			cerr << "Thread pool size:" << threadPoolSize << endl;
			for(unsigned i=0; i<threadPoolSize; ++i)
				workers.push_back(new StratorWorker(this));
			/// trigger the
			for(unsigned i=0; i<threadPoolSize; ++i){
				int retVal = pthread_create(&tids[i], 0, stratorWorkerHelper, workers[i]);
				assert(retVal == 0 && "Problem with creating the threads");
			}

			/// Synchronize the threads
			for(unsigned i=0; i<threadPoolSize; ++i){
				int retVal = pthread_join(tids[i], 0);
				assert(retVal == 0 && "Problem with joining the threads");
			}
		}
	}

	if(ReportUnprocessed){
		/// list all the unprocessed entry point functions
		cerr << "Following entry point functions were not processed: " << endl;
		FunctionMap::iterator fMIt;
		for(fMIt = functionMap.begin(); fMIt != functionMap.end(); ++fMIt){
			if(!fMIt->second)
				cerr << fMIt->first << endl;
		}
	}

	/// We have gathered race detection data, now report
	reportRaces();
	/// We did not modify the module, return false
	return false;
}

set<Strator::StratorWorker::LockSet>& Strator::StratorWorker::traverseFunction(const Function& f, LockSet lockSet){
	//#ifdef DETAILED_DEBUG
	cerr << " Traversing: " << f.getName().str() << endl;
	//#endif
	if("signal_threads" == f.getName().str())
		cerr << "signal" << endl;
	/// This should be OK even if not thread safe
	//TODO: Ali: why OK if not thread safe ?!
	parent->functionMap[f.getName().str()] = true;
	/// If the size of a basic block is 0, then we are in a function declaration.
	if (f.size() == 0){
		set<StratorWorker::LockSet>* emptySet = new set<StratorWorker::LockSet>();
		return *emptySet;
	}

	StratorFunction* sFunc = getStratorFunction(&f);

	/// Check if the function is in the cache
	vector<FunctionCacheEntry>::iterator it;
	for(it = sFunc->functionCache.functionCacheEntries.begin();
			it != sFunc->functionCache.functionCacheEntries.end(); ++it){
		if(it->entryLockSet == lockSet){
			return it->exitLockSets;
		}
	}
	/// Simply ignore recursion
	if(sFunc->onStack){
		set<StratorWorker::LockSet>* emptySet = new set<StratorWorker::LockSet>();
		return *emptySet;
	}

	sFunc->onStack= true;

	/// The function was not in the cache, so a new cache entry is being created for it
	FunctionCacheEntry* functionCacheEntry = new FunctionCacheEntry();
	functionCacheEntry->entryLockSet = lockSet;

	/// Start traversing the statements with the beginning statement of the function
	Function::const_iterator firstBB = f.begin();
	BasicBlock::const_iterator firstInstr = firstBB->begin();
	functionCacheEntry->exitLockSets = traverseStatement(f, firstInstr, lockSet, lockSet);

	/// We processes the current function, it is no longer on the stack
	sFunc->onStack= false;

	/// Add the exit lockset to the summary cache
	sFunc->functionCache.functionCacheEntries.push_back(*functionCacheEntry);

	return functionCacheEntry->exitLockSets;
}




set<Strator::StratorWorker::LockSet>& Strator::StratorWorker::traverseStatement(const Function& f, 
		const BasicBlock::const_iterator& inst,
		const Strator::StratorWorker::LockSet& entryLockSet,
		Strator::StratorWorker::LockSet lockSet){
//	cerr << "@: ";
//	llvm::errs() << *inst;
//	cerr << endl;
	//printMultithreaded();
	if(stratorStatementMap.find(&(*inst)) == stratorStatementMap.end()){
		stratorStatementMap[&(*inst)] = new StratorStatement();
	}
	StratorStatement* sStmt= stratorStatementMap[&(*inst)];

	/// Check the statement cache, if the lockset is in the cache, don't analyze
	if(sStmt->statementCache.isInCache(entryLockSet, lockSet)){
		set<Strator::StratorWorker::LockSet>* emptySet = new set<Strator::StratorWorker::LockSet>();
		return *emptySet;
	}

	/// Otherwise add (entryLockSet, lockSet) to the cache
	sStmt->statementCache.addToCache(entryLockSet, lockSet);

	/// Terminator instructions for our analysis:
	/// Ret
	/// IndirectBr: Probably this is one such instruction for us,
	/// as we evaluate more systems, we'll see what happens if we use it
	if(inst->getOpcode() == Instruction::IndirectBr){
		printLocation(*inst);
		assert(false && "We hit an indirect branch\n");
	}

	if(inst->getOpcode() == Instruction::Ret){
		set<Strator::StratorWorker::LockSet>* returnSet = new set<Strator::StratorWorker::LockSet>();
		returnSet->insert(lockSet);
		return *returnSet;
	}

	vector<LockSet>* workingList = new vector<StratorWorker::LockSet>();

	/// TODO: What do we do with invoke?
	if(inst->getOpcode() == Instruction::Call) {
		const CallInst* callInst = dyn_cast<CallInst>(&(*inst));
		assert(callInst && "Call inst pointer is NULL!!!, this shouldn't have happened");
		Function* calledFunc = callInst->getCalledFunction();

		StratorFunction* sFunc = NULL;
		if(UpwardPropagationLevel != 0 ){
			sFunc = getStratorFunction(calledFunc);
			sFunc->currentCaller = &f;
		}

		/// Since we don't have a lock intrinsic or anything like that,
		/// we will discover locks and unlocks upon function calls.
		assert((((int)inst->getNumOperands())-1) >= 0 &&
				"Something wrong with the operands of call instruction");
		if(isMultiThreadedAPI(inst->getOperand(inst->getNumOperands() -1 ))){
			multithreadedFunctionMap[f.getName().str()] = true;
			/// Now check if we also want to propagete the "multithreadedness" to callers:
			/// If the called function is a thread-related function, then the parent function(s)
			/// are added to a a multithreadedAPI set depending on the propagation level which is
			/// checked in subsequent calls to isMultiThreadedAPI
			if(UpwardPropagationLevel != 0){
				/// move up to the parents of this call to mark all of them as multithreaded
				for(unsigned i=0; i<UpwardPropagationLevel; ++i){
					const Function* caller = sFunc->currentCaller;
					if(!caller)
						break;
					else {
						/// The upper level function becomes a multithreaded API, son any
						/// subsequent caller of such functions also become multithreaded.
						multithreadedAPISet.insert(caller);
						/// They also become multithreaded functions themselves
						multithreadedFunctionMap[caller->getName().str()] = true;
						sFunc = getStratorFunction(caller);
					}
				}
			}
		}

		if(calledFunc){
			if (isLock(calledFunc)){
				Lock l = getLockValue(&(*inst));

				if(PrintLockUnlock)
					cerr << "lock inst:" << l << endl;

				if(l)
					lockSet.insert(l);
			} else if (isUnLock(calledFunc)){
				Lock l = getLockValue(&(*inst));

				if(PrintLockUnlock)
					cerr << "unlock inst:" << l << endl;

				if(l)
					lockSet.erase(l);
			} else{
				/// Now check if we have a resolved call, that is a direct function call
				/// We have a resolved function call
				if(isThreadCreationFunction(calledFunc->getName().str())) {
					/// If the created function is a thread creation function we
					/// need to find the worker function operand and traverse it
					calledFunc = getThreadWorkerFunction(callInst);
				}

				/// Check if the current function is a multithreaded API
				if(multithreadedFunctionMap[f.getName().str()]){
					/// If so, the current callee also becomes multithreaded
					multithreadedFunctionMap[calledFunc->getName().str()] = true;
				}

				set<StratorWorker::LockSet>& functionLockSets = traverseFunction(*calledFunc, lockSet);
				/// Maybe the called function was multi threaded: If so, the caller becomes multi
				/// threaded from this point on if the following two lines are uncommented.
				/// However, this support adds a large number of false positives
				/*
          if(multithreadedFunctionMap[calledFunc->getName().str()])
          multithreadedFunctionMap[f.getName().str()] = true;
				 */
				workingList->insert(workingList->begin(), functionLockSets.begin(), functionLockSets.end());
			}
		} else {
			/// call is not resolved, process the lockset at hand as the working set
			workingList->insert(workingList->begin(), lockSet);
		}
	}

	/// TODO: We can leverage the llvm function onlyreadsmemory maybe? At least look at the code
	if(inst->getOpcode() == Instruction::Load || inst->getOpcode() == Instruction::Store){
		detectRaces(*inst, (inst->getOpcode() == Instruction::Store), lockSet);
	}

	/// If this is the last instruction of the current BB
	/// it means from here we have successor nodes in the control flow.
	set<StratorWorker::LockSet>* returnSet = new set<StratorWorker::LockSet>();
	BasicBlock* bb = const_cast<BasicBlock*>(inst->getParent());
	/// TODO: try to check for terminator instruction here
	vector<StratorWorker::LockSet>::iterator workingListLockset;

	if(workingList->size() > 0){
		if(&(bb->back()) == &(*inst)){
			for(workingListLockset = workingList->begin();
					workingListLockset != workingList->end(); ++workingListLockset)
				for(succ_iterator child = succ_begin(bb), end = succ_end(bb);
						child != end; ++child){
					BasicBlock::iterator firstInstr = child->begin();
					set<Strator::StratorWorker::LockSet>& statementLockSets = traverseStatement(f, firstInstr, entryLockSet, *workingListLockset);
					returnSet->insert(statementLockSets.begin(), statementLockSets.end());
				}
		} else{
			/// not the last instruction, simply instruction pointer to the next instruction
			BasicBlock::const_iterator nextInst = inst;
			++nextInst;
			for(workingListLockset = workingList->begin();
					workingListLockset != workingList->end(); ++workingListLockset){
				set<Strator::StratorWorker::LockSet>& statementLockSets = traverseStatement(f, nextInst, entryLockSet, *workingListLockset);
				returnSet->insert(statementLockSets.begin(), statementLockSets.end());
			}
		}
	} else{
		if(&(bb->back()) == &(*inst)){
			for(succ_iterator child = succ_begin(bb), end = succ_end(bb);
					child != end; ++child){
				BasicBlock::iterator firstInstr = child->begin();
				set<LockSet>& statementLockSets = traverseStatement(f, firstInstr, entryLockSet, lockSet);

				returnSet->insert(statementLockSets.begin(), statementLockSets.end());
			}
		} else{
			BasicBlock::const_iterator nextInst = inst;
			++nextInst;
			set<Strator::StratorWorker::LockSet>& statementLockSets = traverseStatement(f, nextInst, entryLockSet, lockSet);

			returnSet->insert(statementLockSets.begin(), statementLockSets.end());
		}
	}

	return *returnSet;
}

bool Strator::StratorWorker::isLock(Function* calledFunc){
	bool retVal = false;
	if(calledFunc->getName().str() == "pthread_mutex_lock" ||
			calledFunc->getName().str() == "pthread_mutex_trylock"){
		retVal = true;
	}
	return retVal;
}

bool Strator::StratorWorker::isUnLock(Function* calledFunc){
	bool retVal = false;
	if(calledFunc->getName().str() == "pthread_mutex_unlock"){
		retVal = true;
	}
	return retVal;
}

void Strator::StratorWorker::detectRaces(const Instruction& inst, bool isStore, LockSet& lockSet){
	string fName = inst.getParent()->getParent()->getName().str();
	AccessType* accessType = new AccessType(isStore, multithreadedFunctionMap[fName], &inst, lockSet);
	Instruction* defInst = const_cast<Instruction*>(&inst);
	Value* operand = NULL;

	cerr << "here!" << endl;

	if(isStore){
		assert(inst.getNumOperands() == 2 && "Store should have 2 operands");
		operand = parent->getDefOperand(defInst, defInst->getOperand(1));
	} else{
		assert(inst.getNumOperands() == 1 && "Load should have 1 operand");
		operand = parent->getDefOperand(defInst, defInst->getOperand(0));
	}

	if(operand){
		if(isa<GlobalVariable>(operand)){
			/// Ignore constant globals
			if( ((GlobalVariable*)(operand))->isConstant())
				return;
		}

#ifdef DETAILED_DEBUG
		cerr << "Recording access in function: " << fName << " mt: " << multithreadedFunctionMap[fName] << endl;
		cerr << "location:" << parent->getLocation(&inst) << endl;
		cerr << "Variable: " << operand->getName().str() << endl;
		cerr << "And instruction: ";
		llvm::errs() << inst;
		cerr << endl;
		cerr << "Lockset:" << endl;
		Strator::StratorWorker::LockSet::iterator it;
		for(it = lockSet.begin(); it != lockSet.end(); ++it){
			cerr << "   lock: " << *it << endl;
		}
#endif

		valueToAccessTypeMap[operand].push_back(accessType);
	}
}

void Strator::reportRaces(){
	switch (RaceDetectorLevel){
	case 1:
		/// Simple reporter: Report all global accesses in allegedly
		/// multithreaded contexts that occur without a common lock held
		reportLevel1Races();
		break;
	case 2:
		/// Tier-2 detector
		assert(false && "Level not implemented");
		break;
	case 3:
		/// Tier-3 detector
		assert(false && "Level not implemented");
		break;
	}
}


static std::string GetFunctionName(const Value* v){
	if (isa<Instruction>(v)){
		return dyn_cast<Instruction>(v)->getParent()->getParent()->getName().str();
	}else if (isa<Argument>(v)){
		return dyn_cast<Argument>(v)->getParent()->getName().str();
	}else
		return "global";
}


//TODO: make this part clean
//=================================================================
static void printValuePair(const char *Msg, bool P, const Value *V1,
		const Value *V2, const Module *M) {
	if (P) {
		std::string o1, o2;
		{
			raw_string_ostream os1(o1), os2(o2);
			os1 << GetFunctionName(V1) << ": ";
			os2 << GetFunctionName(V2) << ": ";
			WriteAsOperand(os1, V1, true, M);
			WriteAsOperand(os2, V2, true, M);
		}


		errs() << "  " << Msg << ":\t"
				<< o1 << ", "
				<< o2 << "\n";
	}
}

static void printValuePair(const char *Msg, bool P, const Value *V1,
		const Value *V2, const Module *M, raw_ostream& file) {
	if (P) {
		std::string o1, o2;
		{
			raw_string_ostream os1(o1), os2(o2);
			os1 << GetFunctionName(V1) << ": ";
			os2 << GetFunctionName(V2) << ": ";
			WriteAsOperand(os1, V1, true, M);
			WriteAsOperand(os2, V2, true, M);
		}

		file << "  " << Msg << ":\t"
				<< o1 << ", "
				<< o2 << "\n";
	}
}
//=================================================================

void Strator::investigateAccesses(Strator::StratorWorker::ValueToAccessTypeMap::iterator valIt1,
		Strator::StratorWorker::ValueToAccessTypeMap::iterator valIt2, raw_ostream& file, int& raceCount){
	//printValuePair("checking ", true, valIt1->first, valIt2->first, NULL);
	std::vector<Strator::StratorWorker::AccessType*>::iterator accIt1, accIt2;
	for(accIt1 = valIt1->second.begin(); accIt1 != valIt1->second.end(); ++accIt1){
		for(accIt2 = valIt2->second.begin(); accIt2 != accIt1 && accIt2 != valIt2->second.end(); ++accIt2){
			if((*accIt1)->isStore || (*accIt2)->isStore){
				if((*accIt1)->isMultiThreaded && (*(accIt2))->isMultiThreaded){
//					/// if the accessed values are local and, it they are not modified, there is no race
//					if(localValueInfo){
//						LocalIdentifier::LocalValueInfo::iterator localValIt;
//						localValIt = localValueInfo->find(valIt->first);
//						if(localValIt != localValueInfo->end()){
//							if(!localValIt->second){
//								/// TODO: Here we ignore some definitely local accesses
//								continue;
//							}
//						}
//					}

					string loc1 = getLocation((*accIt1)->instruction);
					string loc2 = getLocation((*accIt2)->instruction);
#ifdef DETAILED_DEBUG
					cerr << "Accesses: " << endl;
					cerr << " " << loc1 << ": "
							<< loadOrStore((*accIt1)->isStore) << endl;
					cerr << " " << loc2 << ": "
							<< loadOrStore((*accIt2)->isStore) << endl << endl;
#endif

					/// Otherwise check the locksets
					if(!doLockSetsIntersect((*accIt1)->lockSet, (*(accIt2))->lockSet)){

						string key = valIt1->first->getName().str() + loc1 +
								loadOrStore((*accIt1)->isStore) + valIt2->first->getName().str() + loc2 + loadOrStore((*accIt2)->isStore);
						string inverseKey = valIt2->first->getName().str() + loc2 +
								loadOrStore((*accIt2)->isStore) + valIt1->first->getName().str() + loc1 + loadOrStore((*accIt1)->isStore);
						//cerr << "befor cacke checking "  << key << "--" << inverseKey << endl;
						if(raceCache.find(key) == raceCache.end() && raceCache.find(inverseKey) == raceCache.end()){
							//cerr << " not in cache" << endl;
							// Filter races from the standart libraries
							//if(notFiltered(valIt1->first->getName().str()) && notFiltered(valIt2->first->getName().str())){
								if(notFiltered(key)){
									cerr << " yohoo" << raceCount << endl;
									raceCache.insert(key);
									instrumentCache.push_back(make_pair((*accIt1)->instruction, (*accIt2)->instruction));


									/*
	                      llvm::errs() << "inst: " << *((*accIt1)->instruction);
	                      cerr << endl;
	                      llvm::errs() << "inst: " << *((*accIt2)->instruction);
	                      cerr << endl;
									 */
									/*
	                    cerr << "Potential race \'" << raceCount << "\' on variable: \""
	                         << valIt->first->getName().str() << "\" at:" << endl;
	                    cerr << " " << loc1 << ": " << loadOrStore((*accIt1)->isStore) << endl;
	                    cerr << " " << loc2 << ": " << loadOrStore((*accIt2)->isStore) << endl << endl;
									 */
									printValuePair("Potential race", true, valIt1->first, valIt2->first, NULL, file);
//									file << "Potential race \'" << raceCount << "\' on variable: \""
//											<< valIt1->first->getName().str() << "\" and \"" << valIt2->first->getName().str()
//											<< "\" at:" << endl;
									file << " " << loc1 << ": " << loadOrStore((*accIt1)->isStore) << "\n";
									if (ReportLLVMInstruction){
										file << "   " << *((*accIt1)->instruction) << "\n";
									}
									file << " " << loc2 << ": " << loadOrStore((*accIt2)->isStore) << "\n";
									if (ReportLLVMInstruction){
										file << "   " << *((*accIt2)->instruction) << "\n";
									}
									++raceCount;
								}
							//}
						}
					}
				}
			}
		}
	}


}

void Strator::reportLevel1Races(){
	/// use the following for debugging
	/// printValueToAccessTypeMap();
	int raceCount = 0;
	Strator::StratorWorker::ValueToAccessTypeMap::iterator valIt;
	/// Merge the results from all the Strator Workers to get a global
	/// ValueToAccessTypeMap to create the final value to access type map
	mergeValueToAccessTypeMaps();

	if(DebugAliasAnalysis){

		if(!aa){
			cerr << "Alias analysis results not gathered" << endl;
		} else {
			Strator::StratorWorker::ValueToAccessTypeMap::iterator valIt2;
			for(valIt = finalValueToAccessTypeMap.begin(); valIt != finalValueToAccessTypeMap.end(); ++valIt)
				for(valIt2 = finalValueToAccessTypeMap.begin(); valIt2 != valIt; ++valIt2)
					if(aa->alias(valIt->first, valIt2->first)){

						printValuePair("May alias", true, &*(valIt->first), &*(valIt2->first), NULL);

						/*
						string scp1,scp2;
						Instruction* temp = dyn_cast<Instruction>(valIt->first);
						if (temp)
							scp1 = "@" + temp->getParent()->getParent()->getName().str();
						temp = dyn_cast<Instruction>(valIt2->first);
						if (temp)
							scp2 = "@" + temp->getParent()->getParent()->getName().str();
						cerr << valIt->first->getName().str() << scp1 << " and " << valIt2->first->getName().str() << scp2 << " may alias" << endl;
						 */
					}
		}
	}

	std::string fileError;
	raw_fd_ostream file("races.txt", fileError);

	for(valIt = finalValueToAccessTypeMap.begin(); valIt != finalValueToAccessTypeMap.end(); ++valIt){
		/// If there has been a single access, there is no race
		if(valIt->second.size() == 1){
			continue;
		}

		investigateAccesses(valIt, valIt, file, raceCount);

	}


	if (UseAliasAnalysis){
		assert(aa && "Alias analysis results not gathered");
		Strator::StratorWorker::ValueToAccessTypeMap::iterator valIt2;
		for(valIt = finalValueToAccessTypeMap.begin(); valIt != finalValueToAccessTypeMap.end(); ++valIt)
			for(valIt2 = finalValueToAccessTypeMap.begin(); valIt2 != valIt; ++valIt2)
				if(aa->alias(valIt->first, valIt2->first))
					investigateAccesses(valIt, valIt2, file, raceCount);
	}

	file.close();
	assert(fileError.empty() && fileError.c_str());

	cerr << "Total number of races: " << raceCount << endl;
}

void Strator::mergeValueToAccessTypeMaps(){
	/// copy the first worker's map directly
	finalValueToAccessTypeMap = workers[0]->valueToAccessTypeMap;
	/// go through rest of the worker's maps
	for(unsigned i=1; i<threadPoolSize; ++i){
		Strator::StratorWorker::ValueToAccessTypeMap::iterator valIt;
		for(valIt = workers[i]->valueToAccessTypeMap.begin();
				valIt != workers[i]->valueToAccessTypeMap.end(); ++valIt){
			if(finalValueToAccessTypeMap.find(valIt->first) == finalValueToAccessTypeMap.end()){
				/// The value item is not in the map
				finalValueToAccessTypeMap[valIt->first] = valIt->second;
			} else {
				/// The item is in, therefore we should append the accesses to the
				finalValueToAccessTypeMap[valIt->first].insert(finalValueToAccessTypeMap[valIt->first].end(),
						valIt->second.begin(), valIt->second.end());
			}
		}
	}
}

/// If there are more thread creation API functions, add them here
bool Strator::StratorWorker::isThreadCreationFunction(string fName){
	bool retVal = false;
	if(fName == "pthread_create")
		return true;
	return retVal;
}

Function* Strator::StratorWorker::getThreadWorkerFunction(const CallInst* callInst){
	/// Need a switch case whenever if we have more APIs than pthreads
	unsigned num = callInst->getNumOperands();
	assert(num == 5 && "There should be five operands in a call to pthread_create");
	/// Third operand is the thread creation function
	Value* operand = callInst->getOperand(2);
	if(isa<Function>(*operand))
		return (Function*)operand;
	else if (PointerType::classof(operand->getType())){
		Value * pointedTo = ((ConstantExpr *)(operand))->getOperand(0);
		if(isa<Function>(*pointedTo))
			return (Function*)pointedTo;
		else
			assert(false && "Thread worker function pointer not resolved, something is wrong");
	}
	else
		assert(false && "Thread worker function pointer not resolved, something is wrong");
}

Value* Strator::StratorWorker::getLockValue(const Instruction* inst){
	if (const CallInst* CI = dyn_cast<CallInst>(inst)){
		int numOperands = CI->getNumArgOperands();
			Value* operand = NULL;
			switch (numOperands){
			case 1: {
				if(isa<Instruction>(inst->getOperand(0))){
					Instruction* defInst = dyn_cast<Instruction>(CI->getArgOperand(0));
					operand = parent->getDefOperand(defInst,
							defInst->getOperand(0));
				} else if(isa<GlobalVariable>(CI->getArgOperand(0))){
					operand = CI->getArgOperand(0);
				} else if(isa<GEPOperator>(CI->getArgOperand(0))){
					operand = parent->getDefOperand(const_cast<Instruction*>(inst),
							CI->getArgOperand(0));
				}else{
					//It may be just local value, what's the problem ?
					operand = CI->getArgOperand(0);
				}
				break;
			}
			default:
				inst->dump();
				cerr << endl;
				cerr << "operand num: " << numOperands << endl;
				assert(false && "Case not handled !!!");
				break;
			}
			/// Uncomment the following line if the "lock operand can't be NULL"
			/// assertion fires to see the culprit insturction causing this

			if(!operand){
				llvm::errs() << "culprit: " << *parent->culprit;
				cerr << endl;
			}

			// assert(operand && "The returned lock operand can't be NULL");
			return operand;

	}else{
		inst->dump();
		assert(false && "Unsupported lock/unlock instruction" );
	}
//	int numOperands = inst->getNumOperands();
//	Value* operand = NULL;
//	switch (numOperands){
//	case 2: {
//		if(isa<Instruction>(inst->getOperand(0))){
//			Instruction* defInst = dyn_cast<Instruction>(inst->getOperand(0));
//			operand = parent->getDefOperand(defInst,
//					defInst->getOperand(0));
//		} else if(isa<GlobalVariable>(inst->getOperand(0))){
//			operand = inst->getOperand(0);
//		} else if(isa<GEPOperator>(inst->getOperand(0))){
//			operand = parent->getDefOperand(const_cast<Instruction*>(inst),
//					inst->getOperand(0));
//		}else{
//			cerr << "op1: " << inst->getOperand(0)->getName().str() << endl;
//			cerr << "op2: " <<inst->getOperand(1)->getName().str() << endl;
//			llvm::errs() << *inst;
//			cerr << endl;
//			assert ( false && "Unknown type!");
//		}
//		break;
//	}
//	default:
//		llvm::errs() << *inst;
//		cerr << endl;
//		cerr << "operand num: " << numOperands << endl;
//		assert(false && "Case not handled !!!");
//		break;
//	}
//	/// Uncomment the following line if the "lock operand can't be NULL"
//	/// assertion fires to see the culprit insturction causing this
//
//	if(!operand){
//		llvm::errs() << "culprit: " << *parent->culprit;
//		cerr << endl;
//	}
//
//	// assert(operand && "The returned lock operand can't be NULL");
//	return operand;
}

bool Strator::doLockSetsIntersect(Strator::StratorWorker::LockSet& ls1, 
		Strator::StratorWorker::LockSet& ls2){
#ifdef DETAILED_DEBUG
	cerr << " Locksets:" << endl;
#endif
	bool retVal = false;
	Strator::StratorWorker::LockSet::iterator ls1It, ls2It;
	for(ls1It = ls1.begin(); ls1It != ls1.end(); ++ls1It){
		for(ls2It = ls2.begin(); ls2It != ls2.end(); ++ls2It){
#ifdef DETAILED_DEBUG
			cerr << "    lock1: " << (*ls1It) << endl;
			cerr << "    lock2: " << (*ls2It) << endl;
#endif
			if(*ls1It == *ls2It){
				retVal = true;
				break;
			}
		}
	}
#ifdef DETAILED_DEBUG
	cerr << endl;
	cerr << "returning " << retVal << endl;
#endif
	return retVal;
}

bool Strator::StratorWorker::isMultiThreadedAPI(const Value* func){
	bool retVal = false;
	if(UpwardPropagationLevel == 0) {
		/// The last operand of a call instruction is the name of the called function
		string calledFuncName = func->getName().str();
		if(calledFuncName.find("pthread") != std::string::npos ||
				calledFuncName.find("apr_thread") != std::string::npos)
			if (calledFuncName.find("_init") == std::string::npos &&
					calledFuncName.find("_get") == std::string::npos &&
					calledFuncName.find("_set") == std::string::npos)
				retVal = true;
	} else{
		if(multithreadedAPISet.find(func) != multithreadedAPISet.end()){
			retVal = true;
		} else{
			/// The last operand of a call instruction is the name of the called function
			string calledFuncName = func->getName().str();
			if(calledFuncName.find("pthread") != std::string::npos ||
					calledFuncName.find("apr_thread") != std::string::npos)
				if (calledFuncName.find("_init") == std::string::npos &&
						calledFuncName.find("_get") == std::string::npos &&
						calledFuncName.find("_set") == std::string::npos)
					retVal = true;
		}
	}
	return retVal;
}

bool Strator::notFiltered(string str){
	bool retVal = true;
	if(str == "")
		retVal = false;
	else if(str.rfind("include/c++") != std::string::npos)
		retVal = false;
	return retVal;
}

inline string Strator::loadOrStore(bool isStore){
	if(isStore)
		return "STORE";
	else
		return "LOAD";
}

Value* Strator::getDefOperand(Instruction* defInst, Value* operand){  
	/// TODO: try to move this mutex solely to GEPFactory methods, the rest is pretty much
	/// immutable
	return operand;
//	pthread_mutex_lock(&operationMutex);
//	cerr << " at getDefOperand " << operand->hasName() << endl;
//	if (operand->hasName())
//		cerr << " it has name and its name is " << operand->getName().str() << ":" << operand->getValueName()->first().str() << endl;
//	Value* retVal = operand;
//	if(!operand->hasName()){
//		cerr << " so value does not have name " << endl;
//		set<Value*> visited;
//		Instruction* process = NULL;
//		while(useDefMap->find(defInst) != useDefMap->end()){
//			/// TODO: convert useDefMap to have constant insturction keys and values
//			llvm::errs() << *process;
//			cerr << endl;
//			defInst = (*useDefMap)[defInst];
//			// Don't run in circles in the use-def chain, maintain a visited insturctions list
//			if(visited.find(defInst) != visited.end())
//				break;
//			if(isa<PHINode>(defInst))
//				break;
//			visited.insert(defInst);
//			process = defInst;
//		}
//		llvm::errs() << "::" << *process << "\n";
//		if(process && GetElementPtrInst::classof(process)){
//			GetElementPtrInst* ptr = dyn_cast<GetElementPtrInst>(process);
//			// iterate over indices
//			std::vector<int> indices;
//			if(ptr->hasAllConstantIndices()){
//				for (User::op_iterator it = ptr->idx_begin(), ie = ptr->idx_end(); it != ie; ++it){
//					ConstantInt *idx = cast<ConstantInt>(*it);
//					indices.push_back(idx->getZExtValue());
//				}
//			}
//			if(ptr->getPointerOperand()->hasName())
//				retVal = GEPFactory->getGEPWrapperValue(ptr->getPointerOperand(), indices);
//		} else if(isa<GEPOperator>(operand)){
//			GEPOperator* ptr = dyn_cast<GEPOperator>(operand);
//
//			std::vector<int> indices;
//			if(ptr->hasAllConstantIndices()){
//				for (User::op_iterator it = ptr->idx_begin(), ie = ptr->idx_end(); it != ie; ++it){
//					ConstantInt *idx = cast<ConstantInt>(*it);
//					indices.push_back(idx->getZExtValue());
//				}
//			}
//			if(ptr->getPointerOperand()->hasName())
//				retVal = GEPFactory->getGEPWrapperValue(ptr->getPointerOperand(), indices);
//		} else if(process && LoadInst::classof(process)){
//			cerr << "it is a load, yeeepeeee" << endl;
//			retVal = process->getOperand(0);
//		} else{
//			retVal = NULL;
//			culprit = defInst;
//			/// TODO: log odd instructions to a file to investigate them later
//			/*
//        llvm::errs() << "ODD be: " << *defInst << " process: " << process;
//        cerr << endl;
//			 */
//		}
//	}
//	pthread_mutex_unlock(&operationMutex);
//	return retVal;
}

Strator::StratorWorker::StratorFunction* Strator::StratorWorker::getStratorFunction(const Function* f){
	/// Check to see if we have a a wrapper for this function already
	if(stratorFunctionMap.find(f) == stratorFunctionMap.end()){
		stratorFunctionMap[f] = new StratorFunction();
	}
	return stratorFunctionMap[f];
}

string Strator::getLocation(const Instruction* inst){
	MDNode *node = inst->getMetadata("dbg");
	DILocation loc(node);
	unsigned line = loc.getLineNumber();
	StringRef file = loc.getFilename();
	stringstream locStream;
	locStream << line;
	string locStr = locStream.str();
	string location(file.str() + ": l." + locStr);

	return location;
}

/// This function will bootstrap race detection per entry point
void* Strator::StratorWorker::work(void* arg) {
	Function* f = NULL;
	while(parent->getTask(&f) ){
		Strator::StratorWorker::LockSet* lockSet = new LockSet();
		multithreadedFunctionMap[f->getName().str()] = false;
		traverseFunction(*f, *lockSet);
	}
	return 0;
}

/// Get tasks(function pointers) from the collection of external entry points.
/// This is thread-safe as the thread pool workers retrieve items from here concurrently
bool Strator::getTask(Function** f) {
	pthread_mutex_lock(&taskQueueMutex);

	--functionCount;
	if(functionCount % 100 == 0)
		cerr << "\t" << functionCount;
	if(functionCount < 0){
		pthread_mutex_unlock(&taskQueueMutex);
		return false;
	}

	*f = tasks[functionCount];
	pthread_mutex_unlock(&taskQueueMutex);
	return true;
}

/// Helpers
void Strator::StratorWorker::printLocation(const Instruction& inst){
	MDNode *node = inst.getMetadata("dbg");
	DILocation loc(node);
	unsigned line = loc.getLineNumber();
	StringRef file = loc.getFilename();
	// StringRef dir = loc.getDirectory();
	cerr << "Location: " << endl;
	cerr << file.str() << ", line: " << line << endl;
}

void Strator::StratorWorker::printValueToAccessTypeMap(){
	ValueToAccessTypeMap::iterator valIt;
	for(valIt = valueToAccessTypeMap.begin(); valIt != valueToAccessTypeMap.end(); ++valIt){
		/// If there has been a single access there is no race
		if(valIt->second.size() == 1)
			continue;
		cerr << "Access to: " << valIt->first->getName().str() << endl;
		std::vector<Strator::StratorWorker::AccessType*>::iterator accIt;
		for(accIt = valIt->second.begin(); accIt != valIt->second.end(); ++accIt){
			if((*accIt)->isStore)
				cerr << " STORE" << endl;
			else
				cerr << " LOAD" << endl;
		}
		cerr << endl;
	}
}

void Strator::StratorWorker::printMultithreaded(){
	cerr << "\nMultithreaded function map:\n" << endl;
	Strator::StratorWorker::MultithreadedFunctionMap::iterator it;
	for(it = multithreadedFunctionMap.begin(); it != multithreadedFunctionMap.end(); ++it){
		cerr << left << setw(30) << it->first << " " << setw(2) << it->second << endl;
	}
}

void Strator::printLocalModificationInfo(){
//	if(!localValueInfo)
//		return;
//	cerr << "\nLocals' modification status:\n" << endl;
//	LocalIdentifier::LocalValueInfo::iterator it;
//	for(it = localValueInfo->begin(); it != localValueInfo->end(); ++it){
//		cerr << left << setw(30) << it->first->getName().str() << " " << setw(2) << it->second << endl;
//	}
}

void Strator::StratorWorker::printLockSets(set<Strator::StratorWorker::LockSet>& lockSets){
	cerr << " size of cached items: " << lockSets.size() << endl;
	set<Strator::StratorWorker::LockSet>::iterator lsIt;
	int i = 1;
	for(lsIt = lockSets.begin(); lsIt != lockSets.end(); ++lsIt){
		cerr << "  Lockset: " << i << endl;
		Strator::StratorWorker::LockSet::iterator it;
		for(it = lsIt->begin(); it != lsIt->end(); ++it){
			cerr << "   lock: " << *it << endl;
		}
		++i;
	}
}

char Strator::ID = 0;
static RegisterPass<Strator>
Y("strator", "Strator, the static race detector");
