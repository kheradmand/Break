/*
 * LockIntrumenter.cpp
 *
 *  Created on: Aug 21, 2013
 *      Author: ali
 */

//#define INITIALIZE "_Z10initializev"
#define BEFORE_STORE "_Z11beforeStorePvi"
#define AFTER_STORE "_Z10afterStorePvi"
#define BEFORE_LOAD "_Z10beforeLoadPvi"
//#define AFTER_UNLOCK "_Z11afterUnlockP15pthread_mutex_ti"
//#define FINALIZE "_Z8finalizev"


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

class MemInstrumenter : public ModulePass{
public:
	static char ID;
	typedef map<string, int> LocationIndexMap;

	MemInstrumenter() : ModulePass(ID) {
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

		if (locIndex.find(location) == locIndex.end()){
			errs() << " new location: " << location << " assigning " << locNum  << " to it\n";
			locIndex[location] = locNum++;
		}

		return locIndex[location];
	}

	void printLocationIndexMap(){
		ofstream fout("location-index-map.txt");
		for (LocationIndexMap::iterator it = locIndex.begin(); it != locIndex.end(); it++)
			fout <<  it->first << "\t" << it->second << endl;
		fout.close();
	}
	void initLocationIndexMap(){
		ifstream fin("location-index-map.txt");
		string str;
		while (fin >> str){
			int index;
			fin >> index;
			locIndex[str] = index;
			locNum++;
		}
		fin.close();
	}

	virtual bool runOnModule(Module &m) {
		initLocationIndexMap();

		Type* voidstart_t = PointerType::getInt8PtrTy(getGlobalContext());
		Type* params[] = { voidstart_t, IntegerType::getInt32Ty(getGlobalContext()) };
		FunctionType* funcType = FunctionType::get(Type::getVoidTy(getGlobalContext()), params, false);

		Function* beforeStore = Function::Create(funcType, GlobalVariable::ExternalLinkage, BEFORE_STORE, &m);
		Function* afterStore = Function::Create(funcType, GlobalVariable::ExternalLinkage, AFTER_STORE, &m);
		Function* beforeLoad = Function::Create(funcType, GlobalVariable::ExternalLinkage, BEFORE_LOAD, &m);
//		Function* afterUnlock = Function::Create(funcType, GlobalVariable::ExternalLinkage, AFTER_UNLOCK, &m);

//		Function* initialize = Function::Create(FunctionType::get(Type::getVoidTy(getGlobalContext()), false),
//				GlobalVariable::ExternalLinkage, INITIALIZE, &m);
//		Function* finalize = Function::Create(FunctionType::get(Type::getVoidTy(getGlobalContext()), false),
//				GlobalVariable::ExternalLinkage, FINALIZE, &m);

		for (Module::iterator func = m.begin(); func != m.end(); func++){
			for (Function::iterator bb = func->begin(); bb != func->end(); bb++){
				for (BasicBlock::iterator inst = bb->begin(); inst != bb->end(); inst++){
					if (StoreInst* storeInst = dyn_cast<StoreInst>(inst)){
						DEBUG_WITH_TYPE("mem-instrument",
								errs() << "* store " << *storeInst << "\n");

						CastInst* castInst = BitCastInst::CreatePointerCast(
								storeInst->getPointerOperand(), voidstart_t, "",storeInst);

						DEBUG_WITH_TYPE("mem-instrument",
														errs() << "adding " << *castInst << "\n");

						Value* args[] = { castInst,
								ConstantInt::get(
										Type::getInt32Ty(getGlobalContext()), getLocationIndex(inst))
						};
						CallInst* callBeforeStore = CallInst::Create(beforeStore, args);
						CallInst* callAfterStore = CallInst::Create(afterStore, args);
						callBeforeStore->insertBefore(inst);
						callAfterStore->insertAfter(inst);

					}
					else if (StoreInst* loadInst = dyn_cast<StoreInst>(inst)){
						DEBUG_WITH_TYPE("mem-instrument",
								errs() << "/ load " << *loadInst << "\n");

						CastInst* castInst = BitCastInst::CreatePointerCast(
								loadInst->getPointerOperand(), voidstart_t, "",loadInst);

						DEBUG_WITH_TYPE("mem-instrument",
														errs() << "adding " << *castInst << "\n");

						Value* args[] = { castInst->getOperand(0),
								ConstantInt::get(
										Type::getInt32Ty(getGlobalContext()), getLocationIndex(inst))
						};
						CallInst* callBeforeLoad = CallInst::Create(beforeLoad, args);
						callBeforeLoad->insertBefore(inst);

					}
				}
			}
		}

//		Function* main = m.getFunction("main");
//		assert(main && "main is NULL!\n");
//		Instruction& firstInst = main->front().front();
//		Instruction& lastInst = main->back().back();
//
//		CallInst* callInitialize = CallInst::Create(initialize);
//		CallInst* callFinalize = CallInst::Create(finalize);
//
//
//		callInitialize->insertBefore(&firstInst);
//		//TODO: this is silly!
//		callFinalize->insertBefore(&lastInst);

		printLocationIndexMap();

		return true;
	}
};

char MemInstrumenter::ID = 0;
static RegisterPass<MemInstrumenter> LI(
		"mem-instrument", "Instruments code to call function before or after memory accesses",
		true, false);
