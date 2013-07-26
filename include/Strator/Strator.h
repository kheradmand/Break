/*
 * File: Strator.h
 *
 * Author: kasikci
 * You need to chain this analysis with alias analysis to have better results
 */

#include <iostream>
#include <fstream>
#include <map>
#include <set>
#include <pthread.h>

#if LLVM_VERSION_CODE < LLVM_VERSION(3, 3)
#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Target/TargetData.h"
#else
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/AliasAnalysis.h"
#endif
//#include "../LocalIdentifier/LocalIdentifier.h"
//#include "../UseDefBuilder/UseDefBuilder.h"

/// TODO: get rid of the unnecessary locks

namespace llvm{
  class Strator : public ModulePass {
  public:
    /// Strator Types
    typedef std::set<std::string> RaceCache;
    typedef std::vector<std::pair<const Instruction*, const Instruction*> > InstrumentCache;
    typedef std::map<std::string, bool> FunctionMap;

    /// Strator Methods
    void printLocalModificationInfo();

    /// A static function to workaround passing an incompatible function to pthread_create
    static void* stratorWorkerHelper(void* object) {
      return ((StratorWorker*)object)->work(0);
    }

    /// This will retrieve an entry function from the list of all possible entry points in a
    /// thread-safe manner
    bool getTask(Function** f);

    /// Race reporting strategies
    void reportRaces();
    void reportLevel1Races();    
    void mergeValueToAccessTypeMaps();
    inline std::string loadOrStore(bool);
    Value* getDefOperand(Instruction* defInst, 
                         Value* operand);
    bool notFiltered(std::string str);

    std::string getLocation(const Instruction* inst);      
    virtual bool runOnModule(Module &m);
    virtual void getAnalysisUsage(AnalysisUsage& au) const;
    
    Strator() : ModulePass(ID), /*localValueInfo(NULL), useDefMap(NULL),*/ aa(NULL), GEPFactory(NULL),
      functionCount(0), culprit(NULL){
      threadPoolSize = sysconf( _SC_NPROCESSORS_ONLN ) - 4;
      pthread_mutex_init (&taskQueueMutex, 0);
      pthread_mutex_init (&operationMutex, 0);
    }
    class StratorWorker{
    public:
      /// 
      void* work(void* arg);      
      void printMultithreaded();
       
      /// Some data types required by the Strator workers
      typedef Value* Lock;
      typedef std::set<Lock> LockSet;      
      /// Each FunctionCacheEntry contains an entry lockset and a set of exit 
      /// locksets any given function may have multiple such edges
      class FunctionCacheEntry{
      public:
        LockSet entryLockSet;
        std::set<LockSet> exitLockSets;
      };    
      /// This function summary cache maps locksets at function 
      /// entry to locksets at function exit
      class FunctionCache{
      public:
        std::vector<FunctionCacheEntry> functionCacheEntries;
      };      
      /// We need a wrapper around the LLVM function for Strator-specific information
      class StratorFunction {
      public:
        StratorFunction() : onStack(false), currentCaller(NULL){}
        bool onStack;
        FunctionCache functionCache;
        const Function* currentCaller;
      };
      /// Map of LLVM functions to their Strator wrappers
      typedef std::map<const Function*, StratorFunction*> StratorFunctionMap;

      class StatementCacheEntry {
      public:
        StatementCacheEntry(const LockSet& _entryLockSet, const LockSet& _currentLockSet){
          entryLockSet = _entryLockSet;
          currentLockSet = _currentLockSet;
        }
        LockSet entryLockSet;
        LockSet currentLockSet;
      };
      
      class StatementCache {
      public:
        void addToCache(const LockSet& _entryLockset, const LockSet& _currentLockset){
          statementCacheEntries.push_back(StatementCacheEntry(_entryLockset, _currentLockset));
        }
        bool isInCache(const LockSet& _entryLockset, const LockSet& _currentLockset){
          std::vector<StatementCacheEntry>::iterator it;
          for(it = statementCacheEntries.begin(); 
              it != statementCacheEntries.end(); ++it){
            if(_entryLockset == it->entryLockSet && 
               _currentLockset == it->currentLockSet){
              return true;
            }
          }
          return false;
        }
      private:
        std::vector<StatementCacheEntry> statementCacheEntries;
      };
      
      // We need a wrapper around the LLVM instruction for Strator-specific information
      class StratorStatement {
      public:
        StatementCache statementCache;
      };
      
      class AccessType {
      public:
      AccessType(bool _isStore, bool _isMultiThreaded, 
               const Instruction* _instruction, LockSet& _lockSet) : 
        isStore(_isStore), isMultiThreaded(_isMultiThreaded), instruction(_instruction), lockSet(_lockSet) {}
        bool isStore;
        bool isMultiThreaded;
        const Instruction* instruction;
        LockSet lockSet;
      };              
      
      /// we need to map all accessed values to a vector of accesstypes
      typedef std::map<Value*, std::vector<AccessType*> > ValueToAccessTypeMap;
      
      /// Map of LLVM instructions to their Strator wrappers
      typedef std::map<const Instruction*, StratorStatement*> StratorStatementMap;
      
      /// Map of functions to keep track of processed-unprocessed functions
      typedef std::map<std::string, bool> FunctionMap;
            
      typedef std::map<std::string, bool> MultithreadedFunctionMap;
      
      /// This is used to map potentially equivalent get element pointer instructions
      typedef std::map<std::string, Value*> NameToGetElementPtrValue;
      /// 
      StratorWorker(Strator* _parent) : parent(_parent){
        pthread_mutex_init (&operationMutex, 0);
      }
      std::set<LockSet>& traverseFunction(const Function& f, LockSet lockSet);
      std::set<LockSet>& traverseStatement(const Function& f, const BasicBlock::const_iterator& instIter, 
                                           const LockSet& entryLockSet, LockSet lockSet);    
      /// For every value, we keep track of all accesses to that value
      ValueToAccessTypeMap valueToAccessTypeMap;
    private:      
      ///
      Strator *parent;
      ///
      pthread_mutex_t operationMutex;
      /// Every LLVM function has metadata associated with it, 
      /// which is maintained in stratorFunctionMap
      StratorFunctionMap stratorFunctionMap;    
      /// Every LLVM statement has metadata associated with it, 
      /// which is maintained in stratorStatementMap
      StratorStatementMap stratorStatementMap;    
      /// TODO: Is it more correct to have this shared rather thatn per-worker?
      MultithreadedFunctionMap multithreadedFunctionMap;                
      ///
      std::set<const Value*> multithreadedAPISet;
      /// This method has the names of lock statements cached. One needs to
      /// update the method names here if a private synchronization API is used
      bool isLock(Function* calledFunc);
      
      /// This method has the names of unlock statements cached. One needs to
      /// update the method names here if a private synchronization API is used
      bool isUnLock(Function* calledFunc);

      /// Race detection general interface
      void detectRaces(const Instruction& inst, bool isLoad, LockSet& lockSet);
      /// Race detection helpers
      bool isThreadCreationFunction(std::string fName);
      Function* getThreadWorkerFunction(const CallInst* callInst);
      Value* getLockValue(const Instruction* inst);
      bool isMultiThreadedAPI(const Value* func);
      /// This returns the StratorFunction* corresponding to the LLVM function
      StratorFunction* getStratorFunction(const Function* f);
      /// Helpers
      void printLocation(const Instruction& inst);
      void printValueToAccessTypeMap();
      void printLockSets(std::set<StratorWorker::LockSet>& lockSets);
      void printRace(std::string& varName, std::string loc1, std::string& loc2);
      friend class Strator;
    };
   
    /// The GEP Values must be shared accross Strator workers' instances
    class GEPValueFactory{
    public:
      pthread_mutex_t operationMutex;
      class GEPContentWrapper{
      public:
      GEPContentWrapper(Value* _operand, std::vector<int>& _indices): 
        operand(_operand), indices(_indices){}
        
        bool operator<(const GEPContentWrapper& rhs) const{
          bool comp1 = operand < rhs.operand;
          bool comp2 = indices.size() < rhs.indices.size();
          bool comp3 = false;
          
          if(indices.size() == rhs.indices.size())
            for(unsigned i=0; i<indices.size(); ++i){              
              comp3 = comp3 || (indices[i] < rhs.indices[i]);
            }
          return comp1 || 
            ((operand == rhs.operand) && comp2) || 
            ((operand == rhs.operand) && (indices.size() == rhs.indices.size()) && comp3);
        } 
      private:
        Value* operand;
        std::vector<int> indices;        
      };
      
      GEPValueFactory(Module* _module): module(_module){
        pthread_mutex_init (&operationMutex, 0);
      }      
      Value* getGEPWrapperValue(Value* operand, std::vector<int>& indices){      
        // pthread_mutex_lock(&operationMutex);
        GEPContentWrapper wrapper(operand, indices);
        if(GEPWrapperToValue.find(wrapper) == GEPWrapperToValue.end()){
          GlobalVariable* valueWrapper = new GlobalVariable(*module,
                                                            IntegerType::get(module->getContext(), 32),
                                                            false,
                                                            GlobalValue::ExternalLinkage,
                                                            0,
                                                            operand->getName());
          valueWrapper->setAlignment(4);
          GEPWrapperToValue[wrapper] = valueWrapper;
          // pthread_mutex_unlock(&operationMutex);
          return valueWrapper;
        } else{
          Value* valueWrapper =  GEPWrapperToValue[wrapper];
          // pthread_mutex_unlock(&operationMutex);
          return valueWrapper;
        }
        // pthread_mutex_unlock(&operationMutex);
        return NULL;
      }
    private:
      /// Maintain a cache of the getelementptr values so that same GEP instruction contents end
      /// up returning the same Value*. Note that this is supposed to be shared accross StratorWorkers
      std::map<GEPContentWrapper , Value*> GEPWrapperToValue;
      Module* module;
    };

    /// Strator methods continued
    /// Generic race detection functions
    bool doLockSetsIntersect(Strator::StratorWorker::LockSet& ls1, StratorWorker::LockSet& ls2);      
    void investigateAccesses(Strator::StratorWorker::ValueToAccessTypeMap::iterator valIt1,
    		Strator::StratorWorker::ValueToAccessTypeMap::iterator valIt2, raw_ostream& file, int& raceCount);

    /// Strator attrributes
    /// locks to coordinate parallel race detection
    ///
    FunctionMap functionMap;
    /// 
    pthread_mutex_t taskQueueMutex;
    pthread_mutex_t operationMutex;
    std::vector<StratorWorker*> workers;
    ///
    //LocalIdentifier::LocalValueInfo* localValueInfo;
    ///
    //UseDefBuilder::UseDefMap* useDefMap;
    ///
    AliasAnalysis* aa;
    ///
    static char ID;  
    ///
    GEPValueFactory* GEPFactory;
    /// Hold all the tasks
    std::vector<Function*> tasks;
    ///
    long threadPoolSize;    
    ///
    int functionCount;    
    /// The culprit insturction causing a NULL value to be returned from getLockValue, used for debugging
    Instruction* culprit;
    ///
    StratorWorker::ValueToAccessTypeMap finalValueToAccessTypeMap;
    /// Reported race cache to avoid re-reportings
    /// a value to 2-item vector of location string    
    RaceCache raceCache;
    ///
    InstrumentCache instrumentCache;
  };  
}
