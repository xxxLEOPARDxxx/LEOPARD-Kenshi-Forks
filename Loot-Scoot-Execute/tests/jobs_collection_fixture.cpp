#include <Windows.h>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

static unsigned checks=0,allocations=0,queries=0,destructions=0;
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
namespace Ogre {struct Vector3{float x,y,z;Vector3():x(0),y(0),z(0){}};}
struct hand {unsigned id;hand():id(0){}void setNull(){id=0;}};
struct RootObject {unsigned id;bool allowed;RootObject(unsigned n):id(n),allowed(true){}};
struct Character:RootObject {Character(unsigned n):RootObject(n){}};
// Match the actual SDK ownership contract: no buffer-owning destructor,
// count-only clear; the native query allocates and replaces the backing array.
template<class T>struct lektor {
 unsigned count,maxSize;T*stuff;lektor():count(0),maxSize(0),stuff(0){}
 ~lektor(){++destructions;}
 void clear(){count=0;}unsigned size()const{return count;}T operator[](unsigned i)const{return stuff[i];}
 void push(T value){if(count==maxSize){unsigned capacity=maxSize?maxSize*2:10;T*next=(T*)malloc(capacity*sizeof(T));CHECK(next!=0);++allocations;
  for(unsigned i=0;i<count;++i)next[i]=stuff[i];free(stuff);stuff=next;maxSize=capacity;}stuff[count++]=value;}
};
static bool g_effectiveEnableExecuteAction=true;
static void*ou=(void*)1;
struct CanExecuteDiagnostics{};
static CanExecuteDiagnostics MakeCanExecuteDiagnostics(){return CanExecuteDiagnostics();}
static bool CanExecuteFromNativeMenuSelection(Character*,RootObject*target,CanExecuteDiagnostics*,bool){return target&&target->allowed;}
static bool TryReadRootObjectPosition(RootObject*,Ogre::Vector3*){return true;}
static float ComputeExecuteAllRadiusUnits(){return 100;}
static size_t ComputeExecuteAllWorldQueryCap(size_t count){return count>64?128:count*2;}
static bool TryAddExecuteAllTargetToCollection(RootObject*target,hand*handles,uintptr_t*pointers,size_t*count,size_t cap){
 if(*count==cap)return false;handles[*count].id=target->id;pointers[(*count)++]=(uintptr_t)target;return true;
}
static size_t CollectExecuteAllTargets(Character*,RootObject*,bool,hand*,uintptr_t*,size_t);
static Character actor(1);static RootObject primary(2),nearby(3);
static lektor<RootObject*>*lastScratch=0;
static bool nested=false,failQuery=false,throwQuery=false,holdQuery=false;
static HANDLE entered=0,releaseQuery=0;
static bool TryGetCharactersWithinSphere(lektor<RootObject*>*out,const Ogre::Vector3&,float,RootObject*,size_t cap){
 ++queries;lastScratch=out;CHECK(out->size()==0);CHECK(cap<=128);out->push(&nearby);
 if(nested){nested=false;hand handles[8];uintptr_t pointers[8];const unsigned before=queries;
  CHECK(CollectExecuteAllTargets(&actor,&primary,false,handles,pointers,8)==1);CHECK(queries==before);CHECK(handles[0].id==primary.id);}
 if(holdQuery){SetEvent(entered);CHECK(WaitForSingleObject(releaseQuery,2000)==WAIT_OBJECT_0);}
 if(throwQuery)throw 7;
 if(failQuery){out->clear();return false;}
 return true;
}
#include "production_execute_all_collection.inc"
static DWORD WINAPI ConcurrentCollect(void*){hand handles[8];uintptr_t pointers[8];CHECK(CollectExecuteAllTargets(&actor,&primary,false,handles,pointers,8)==2);return 0;}
int main(){
 hand handles[8];uintptr_t pointers[8];
 for(unsigned i=0;i<100;++i){CHECK(CollectExecuteAllTargets(&actor,&primary,false,handles,pointers,8)==2);CHECK(destructions==0);CHECK(lastScratch->size()==0);CHECK(handles[0].id==2&&handles[1].id==3);}
 CHECK(allocations==1&&queries==100);lektor<RootObject*>*first=lastScratch;RootObject**buffer=first->stuff;
 nested=true;CHECK(CollectExecuteAllTargets(&actor,&primary,false,handles,pointers,8)==2);
 CHECK(lastScratch==first&&first->stuff==buffer&&first->size()==0&&allocations==1);
 failQuery=true;CHECK(CollectExecuteAllTargets(&actor,&primary,false,handles,pointers,8)==1);CHECK(first->size()==0);failQuery=false;
 throwQuery=true;bool caught=false;try{CollectExecuteAllTargets(&actor,&primary,false,handles,pointers,8);}catch(int){caught=true;}
 CHECK(caught&&first->size()==0);throwQuery=false;
 CHECK(CollectExecuteAllTargets(&actor,&primary,false,handles,pointers,8)==2);CHECK(allocations==1&&first->size()==0);
 entered=CreateEvent(0,TRUE,FALSE,0);releaseQuery=CreateEvent(0,TRUE,FALSE,0);CHECK(entered&&releaseQuery);holdQuery=true;
 HANDLE worker=CreateThread(0,0,ConcurrentCollect,0,0,0);CHECK(worker!=0);CHECK(WaitForSingleObject(entered,2000)==WAIT_OBJECT_0);
 const unsigned before=queries;CHECK(CollectExecuteAllTargets(&actor,&primary,false,handles,pointers,8)==1);CHECK(queries==before);
 SetEvent(releaseQuery);CHECK(WaitForSingleObject(worker,2000)==WAIT_OBJECT_0);holdQuery=false;CloseHandle(worker);CloseHandle(entered);CloseHandle(releaseQuery);
 CHECK(first->size()==0&&allocations==1);CHECK(CollectExecuteAllTargets(&actor,&primary,false,handles,pointers,8)==2);
 printf("%u extracted collection reuse/reentry/error/concurrency checks passed\n",checks);return 0;
}
