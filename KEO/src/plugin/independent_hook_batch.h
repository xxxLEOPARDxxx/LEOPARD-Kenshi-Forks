#ifndef KEO_INDEPENDENT_HOOK_BATCH_H
#define KEO_INDEPENDENT_HOOK_BATCH_H
#include <stddef.h>

// Startup owner only. This coordinator is deliberately opt-in: no dependent
// feature/patch may publish readiness from a queued hook's creation alone.
class HookBatch {
public:
 enum { CAPACITY=20 };
 typedef int (*Queue)(size_t,void*,void*,void**);
 typedef int (*Apply)(size_t);
 typedef void (*Complete)(int,const char*);
 HookBatch():state(0),count(0),id(0),queue(NULL),apply(NULL){}
 bool Begin(size_t caller,Queue q,Apply a){
  if(state!=0)return false;
  state=3;
  if(!caller||!q||!a)return false;
  id=caller;queue=q;apply=a;state=1;return true;
 }
 bool Collecting()const{return state==1;}
 bool Add(int row,void*target,void*detour,void**original,Complete done){
  return AddRow(row,target,detour,original,done,NULL);
 }
 // The caller owns the result slot through Finish. Null means confirmed.
 bool AddResult(int row,void*target,void*detour,void**original,const char**result){
  return AddRow(row,target,detour,original,NULL,result);
 }
 bool Finish(){
  if(state!=1)return false;
  state=2; // native/callback reentry must never append or reset this transaction
  bool originalsValid=true;
  for(int i=0;i<count;++i){
   Row&r=rows[i];
   try{if(queue(id,r.target,r.detour,r.original)!=0)r.why="batch-queue";}
   catch(...){r.why="batch-queue";}
   // A successful create must supply its trampoline BEFORE any enable.
   if(!r.why&&!*r.original)originalsValid=false;
  }
  bool applied=false;
  if(count&&originalsValid){try{applied=apply(id)==0;}catch(...){}}
  // Apply can partially enable. Retain EVERY original, do not retry through
  // AddHook, and conservatively publish no unconfirmed success in that case.
  state=3;
  for(int i=0;i<count;++i){
   Row&r=rows[i];const char*why=!originalsValid?"batch-original":r.why?r.why:applied?NULL:"batch-apply";
   if(r.result)*r.result=why;
   if(r.done)r.done(r.row,why);
  }
  const bool drained=!count||applied;
  count=0;
  return drained;
 }
private:
 struct Row{int row;void*target;void*detour;void**original;Complete done;const char**result;const char*why;};
 bool AddRow(int row,void*target,void*detour,void**original,Complete done,const char**result){
  if(state!=1||count==CAPACITY||!target||!detour||!original||(!done&&!result))return false;
  for(int i=0;i<count;++i)if(rows[i].row==row)return false;
  Row&r=rows[count++];r.row=row;r.target=target;r.detour=detour;r.original=original;
  r.done=done;r.result=result;r.why=NULL;return true;
 }
 int state,count;size_t id;Queue queue;Apply apply;Row rows[CAPACITY];
 HookBatch(const HookBatch&);HookBatch&operator=(const HookBatch&);
};
// Uses the admitted SDK's queue/apply exports with the same caller module ID.
bool BeginNativeHookBatch(HookBatch& batch);
void FinishNativeHookBatch(HookBatch& batch);
#endif
