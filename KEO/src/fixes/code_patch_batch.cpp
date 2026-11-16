#include "fixes/code_patch_batch.h"
#include <stdint.h>
#include <cstring>
#ifndef KEO_BATCH_PROTECT
#define KEO_BATCH_PROTECT(p,n,f,old) VirtualProtect(p,n,f,old)
#endif
#ifndef KEO_BATCH_FLUSH
#define KEO_BATCH_FLUSH(p,n) FlushInstructionCache(GetCurrentProcess(),p,n)
#endif
#ifndef KEO_BATCH_WRITE
#define KEO_BATCH_WRITE(p,data,n) memcpy(p,data,n)
#endif
namespace code_patch_batch {
static bool Restore(const Edit*e,unsigned n,const DWORD*old){
 bool ok=true;
 // Protection changes on shared pages are a stack, restored in reverse order.
 for(unsigned i=n;i>0;--i)if(old[i-1]){DWORD ignored=0;if(!KEO_BATCH_PROTECT(e[i-1].address,e[i-1].size,old[i-1],&ignored))ok=false;}
 return ok;
}
static bool Rollback(const Edit*e,unsigned changed,unsigned protectedCount,const DWORD*old){
 bool ok=true;
 for(unsigned i=0;i<changed;++i){DWORD ignored=0;if(KEO_BATCH_PROTECT(e[i].address,e[i].size,PAGE_EXECUTE_READWRITE,&ignored))KEO_BATCH_WRITE(e[i].address,e[i].before,e[i].size);else ok=false;}
 for(unsigned i=0;i<changed;++i)if(!KEO_BATCH_FLUSH(e[i].address,e[i].size))ok=false;
 return Restore(e,protectedCount,old)&&ok;
}
static void RawRecovery(const Edit*e,unsigned changed,unsigned protectedCount,const DWORD*old){
 __try{
  for(unsigned i=0;i<changed;++i){DWORD ignored=0;if(VirtualProtect(e[i].address,e[i].size,PAGE_EXECUTE_READWRITE,&ignored))memcpy(e[i].address,e[i].before,e[i].size);}
  for(unsigned i=0;i<changed;++i)FlushInstructionCache(GetCurrentProcess(),e[i].address,e[i].size);
  for(unsigned i=protectedCount;i>0;--i)if(old[i-1]){DWORD ignored=0;VirtualProtect(e[i-1].address,e[i-1].size,old[i-1],&ignored);}
 }__except(EXCEPTION_EXECUTE_HANDLER){}
}
bool Apply(const Edit*edits,unsigned count,code_patch_threads::Set*threads,bool*mayPublish){
 if(mayPublish)*mayPublish=false;
 if(!edits||!count||count>16||!threads||threads->count||!mayPublish)return false;
 for(unsigned i=0;i<count;++i){const Edit&e=edits[i];
  if(!e.address||!e.size||e.size>8||(uintptr_t)e.address>~(uintptr_t)0-e.size||((uintptr_t)e.address>>12)!=(((uintptr_t)e.address+e.size-1)>>12)||memcmp(e.address,e.before,e.size))return false;
  for(unsigned j=0;j<i;++j)if(!((uintptr_t)e.address+e.size<=(uintptr_t)edits[j].address||(uintptr_t)edits[j].address+edits[j].size<=(uintptr_t)e.address))return false;
 }
 DWORD old[16]={0};unsigned protectedCount=0,changed=0;bool armed=false,released=false,clean=true;
 __try{
  __try{
   for(unsigned i=0;i<count;++i){protectedCount=i+1;if(!KEO_BATCH_PROTECT(edits[i].address,edits[i].size,PAGE_EXECUTE_READWRITE,&old[i]))__leave;}
   if(!code_patch_threads::Begin(threads))__leave;
   for(unsigned i=0;i<count;++i)if(memcmp(edits[i].address,edits[i].before,edits[i].size))__leave;
   for(unsigned i=0;i<count;++i){changed=i+1;*mayPublish=true;KEO_BATCH_WRITE(edits[i].address,edits[i].after,edits[i].size);}
   for(unsigned i=0;i<count;++i)if(!KEO_BATCH_FLUSH(edits[i].address,edits[i].size))__leave;
   armed=Restore(edits,protectedCount,old);
  }__finally{
   __try{if(!armed)clean=Rollback(edits,changed,protectedCount,old);}
   __except(EXCEPTION_EXECUTE_HANDLER){clean=false;RawRecovery(edits,changed,protectedCount,old);}
   if(!clean)RawRecovery(edits,changed,protectedCount,old);
   released=code_patch_threads::Finish(threads);
  }
 }__except(EXCEPTION_EXECUTE_HANDLER){armed=false;}
 return armed&&released&&clean;
}
}
