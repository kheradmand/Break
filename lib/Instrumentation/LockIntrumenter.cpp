/*
 * LockIntrumenter.cpp
 *
 *  Created on: Aug 21, 2013
 *      Author: ali
 */

#define INITIALIZE "_Z10initializev"
#define BEFORE_LOCK "_Z10beforeLockP15pthread_mutex_t"
#define AFTER_LOCK "_Z9afterLockP15pthread_mutex_t"
#define AFTER_UNLOCK "_Z11afterUnlockP15pthread_mutex_t"
#define FINALIZE "_Z8finalizev"


#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/GlobalValue.h"

#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"

#include "llvm/Pass.h"

using namespace llvm;

class LockInstrumenter : public ModulePass{
public:
	static char ID;

	LockInstrumenter() : ModulePass(ID) {

	}

	virtual bool runOnModule(Module &m) {
		Type* mutex_t = m.getTypeByName("union.pthread_mutex_t");
		assert(mutex_t && "there is no lock in this program!\n");
		Type* params[] = { PointerType::getUnqual(mutex_t) };
		FunctionType* funcType = FunctionType::get(Type::getVoidTy(getGlobalContext()), params, false);

		Function* beforeLock = Function::Create(funcType, GlobalVariable::ExternalLinkage, BEFORE_LOCK, &m);
		Function* afterLock = Function::Create(funcType, GlobalVariable::ExternalLinkage, AFTER_LOCK, &m);
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
							Value* args[] = { callInst->getArgOperand(0) };

							CallInst* callBeforeLock = CallInst::Create(beforeLock, args);
							CallInst* callAfterLock = CallInst::Create(afterLock, args);

							callBeforeLock->insertBefore(inst);
							callAfterLock->insertAfter(inst);
						}else if (callInst->getCalledFunction()->getName() == "pthread_mutex_unlock"){
							DEBUG_WITH_TYPE("lock-instrument",
									errs() << "- call to pthread unlock " << *callInst << "\n");
							Value* args[] = { callInst->getArgOperand(0) };
							CallInst* callAfterUnlock = CallInst::Create(afterUnlock, args);
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

		return true;
	}
};

char LockInstrumenter::ID = 0;
static RegisterPass<LockInstrumenter> LI(
		"lock-instrument", "Instruments code to call function before and after lock acquisition and release",
		true, false);
