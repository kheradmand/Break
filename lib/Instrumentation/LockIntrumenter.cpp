/*
 * LockIntrumenter.cpp
 *
 *  Created on: Aug 21, 2013
 *      Author: ali
 */

#define INITIALIZE "_Z10initializev"
#define BEFORE_LOCK "_Z10beforeLockP15pthread_mutex_ti"
#define AFTER_LOCK "_Z9afterLockP15pthread_mutex_ti"
#define BEFORE_UNLOCK "_Z12beforeUnlockP15pthread_mutex_ti"
#define AFTER_UNLOCK "_Z11afterUnlockP15pthread_mutex_ti"
#define FINALIZE "_Z8finalizev"


#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Constants.h"

#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"

#include "llvm/DebugInfo.h"
#include "llvm/Pass.h"

#include <map>
#include <string>
#include <fstream>
#include <sstream>


using namespace llvm;
using namespace std;

class LockInstrumenter : public ModulePass{
public:
	static char ID;
	typedef map<string, int> LocationIndexMap;

	LockInstrumenter() : ModulePass(ID) {
		locNum = 0;
	}


	LocationIndexMap locIndex;
	int locNum;
	int getLocationIndex(const Instruction* inst){
		MDNode *node = inst->getDebugLoc().getAsMDNode(inst->getContext());
		DILocation loc(node);

		unsigned line = loc.getLineNumber();
		StringRef file = loc.getFilename();

		stringstream locStream;
		locStream << line;
		string locStr = locStream.str();
		string location(file.str() + ":" + locStr);

		if (locIndex.find(location) == locIndex.end())
			locIndex[location] = locNum++;

		return locIndex[location];
	}

	void printLocationIndexMap(){
		ofstream fout("location-index-map.txt");
		for (LocationIndexMap::iterator it = locIndex.begin(); it != locIndex.end(); it++)
			fout <<  it->first << "\t" << it->second << endl;
		fout.close();
	}


	virtual bool runOnModule(Module &m) {
		Type* mutex_t = m.getTypeByName("union.pthread_mutex_t");
		assert(mutex_t && "there is no lock in this program!\n");
		Type* params[] = { PointerType::getUnqual(mutex_t), IntegerType::getInt32Ty(getGlobalContext()) };
		FunctionType* funcType = FunctionType::get(Type::getVoidTy(getGlobalContext()), params, false);

		Function* beforeLock = Function::Create(funcType, GlobalVariable::ExternalLinkage, BEFORE_LOCK, &m);
		Function* afterLock = Function::Create(funcType, GlobalVariable::ExternalLinkage, AFTER_LOCK, &m);
		Function* beforeUnlock = Function::Create(funcType, GlobalVariable::ExternalLinkage, BEFORE_UNLOCK, &m);
		Function* afterUnlock = Function::Create(funcType, GlobalVariable::ExternalLinkage, AFTER_UNLOCK, &m);

		Function* initialize = Function::Create(FunctionType::get(Type::getVoidTy(getGlobalContext()), false),
				GlobalVariable::ExternalLinkage, INITIALIZE, &m);
		Function* finalize = Function::Create(FunctionType::get(Type::getVoidTy(getGlobalContext()), false),
				GlobalVariable::ExternalLinkage, FINALIZE, &m);

		for (Module::iterator func = m.begin(); func != m.end(); func++){
			for (Function::iterator bb = func->begin(); bb != func->end(); bb++){
				for (BasicBlock::iterator inst = bb->begin(); inst != bb->end(); inst++){
					if (CallInst* callInst = dyn_cast<CallInst>(inst)){
						//errs() << "call to " << callInst->getCalledFunction()->getName() << "\n";
						if (callInst->getCalledFunction()->getName() == "pthread_mutex_lock"){

							DEBUG_WITH_TYPE("lock-instrument",
									errs() << "+ call to pthread lock " << *callInst << "\n");

							Value* args[] = { callInst->getArgOperand(0),
									ConstantInt::get(
											Type::getInt32Ty(getGlobalContext()), getLocationIndex(inst))
							};

							CallInst* callBeforeLock = CallInst::Create(beforeLock, args);
							CallInst* callAfterLock = CallInst::Create(afterLock, args);

							callBeforeLock->insertBefore(inst);
							callAfterLock->insertAfter(inst);

						}else if (callInst->getCalledFunction()->getName() == "pthread_mutex_unlock"){

							DEBUG_WITH_TYPE("lock-instrument",
									errs() << "- call to pthread unlock " << *callInst << "\n");

							Value* args[] = { callInst->getArgOperand(0),
									ConstantInt::get(
											Type::getInt32Ty(getGlobalContext()), getLocationIndex(inst))
							};

							CallInst* callBeforeUnlock = CallInst::Create(beforeUnlock, args);
							CallInst* callAfterUnlock = CallInst::Create(afterUnlock, args);

							callBeforeUnlock->insertBefore(inst);
							callAfterUnlock->insertAfter(inst);

						}
					}
				}
			}
		}

		Function* main = m.getFunction("main");
		assert(main && "main is NULL!\n");
		Instruction& firstInst = main->front().front();
		Instruction& lastInst = main->back().back();

		CallInst* callInitialize = CallInst::Create(initialize);
		CallInst* callFinalize = CallInst::Create(finalize);


		callInitialize->insertBefore(&firstInst);
		//TODO: this is silly!
		callFinalize->insertBefore(&lastInst);

		printLocationIndexMap();

		return true;
	}
};

char LockInstrumenter::ID = 0;
static RegisterPass<LockInstrumenter> LI(
		"lock-instrument", "Instruments code to call function before and after lock acquisition and release",
		true, false);
