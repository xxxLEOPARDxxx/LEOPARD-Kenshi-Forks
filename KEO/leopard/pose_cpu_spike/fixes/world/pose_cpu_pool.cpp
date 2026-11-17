#include "fixes/world/pose_cpu_pool.h"
#include <windows.h>
#include <xmmintrin.h>
namespace pose_cpu_pool {
struct Task {PTP_WORK native;pose_cpu_kernel::Work*input;size_t begin,end;unsigned csr,flags;bool failed;};
static PTP_POOL pool=NULL;
static TP_CALLBACK_ENVIRON environment;
static Task tasks[2]={0};
static volatile LONG gate=0,closing=0;
static void Math(Task*t){try{pose_cpu_kernel::Run(*t->input,t->begin,t->end);}catch(...){t->failed=true;}}
static void Run(Task*t){
 const unsigned saved=_mm_getcsr();
 __try{
  _mm_setcsr(t->csr&~63u);
  __try{Math(t);t->flags=_mm_getcsr()&63u;}
  __except(EXCEPTION_EXECUTE_HANDLER){t->failed=true;}
 }__finally{_mm_setcsr(saved);}
}
static void CALLBACK Callback(PTP_CALLBACK_INSTANCE,void*context,PTP_WORK){Run(static_cast<Task*>(context));}
static void Close(){
 for(unsigned i=0;i<2;++i)if(tasks[i].native){WaitForThreadpoolWorkCallbacks(tasks[i].native,FALSE);CloseThreadpoolWork(tasks[i].native);tasks[i].native=NULL;tasks[i].input=NULL;}
 if(pool){CloseThreadpool(pool);pool=NULL;}
}
static bool Init(){
 if(pool)return true;
 pool=CreateThreadpool(NULL);if(!pool)return false;
 SetThreadpoolThreadMaximum(pool,2);
 if(!SetThreadpoolThreadMinimum(pool,2)){Close();return false;}
 InitializeThreadpoolEnvironment(&environment);SetThreadpoolCallbackPool(&environment,pool);
 for(unsigned i=0;i<2;++i){tasks[i].native=CreateThreadpoolWork(Callback,&tasks[i],&environment);if(!tasks[i].native){Close();return false;}}
 return true;
}
bool Execute(pose_cpu_kernel::Work*w){
 if(!w||!w->vertices||InterlockedCompareExchange(&closing,0,0)||InterlockedCompareExchange(&gate,1,0))return false;
 bool ok=false;const unsigned csr=_mm_getcsr();
 __try{
  if(InterlockedCompareExchange(&closing,0,0)||(csr&0x1f80)!=0x1f80||!Init())__leave;
  for(unsigned i=0;i<2;++i){Task&t=tasks[i];t.input=w;t.begin=w->vertices*i/3;t.end=w->vertices*(i+1)/3;t.csr=csr;t.flags=0;t.failed=false;SubmitThreadpoolWork(t.native);}
  Task owner={0};owner.input=w;owner.begin=w->vertices*2/3;owner.end=w->vertices;owner.csr=csr;Run(&owner);
  for(unsigned i=0;i<2;++i)WaitForThreadpoolWorkCallbacks(tasks[i].native,FALSE);
  ok=!owner.failed&&!tasks[0].failed&&!tasks[1].failed;
  if(ok)_mm_setcsr(csr|owner.flags|tasks[0].flags|tasks[1].flags);
 }__finally{
  // Even exceptional submission/math exits cannot leave a stack-owned job
  // reachable by a callback. Non-alertable waits, no engine/message pumping.
  for(unsigned i=0;i<2;++i)if(tasks[i].native){WaitForThreadpoolWorkCallbacks(tasks[i].native,FALSE);tasks[i].input=NULL;}
  InterlockedExchange(&gate,0);
 }
 return ok;
}
bool Prepare(){
 if(InterlockedCompareExchange(&closing,0,0)||InterlockedCompareExchange(&gate,1,0))return false;
 bool ok=false;__try{if(!InterlockedCompareExchange(&closing,0,0))ok=Init();}__finally{InterlockedExchange(&gate,0);}return ok;
}
void SignalStop(){InterlockedExchange(&closing,1);}
void Stop(){
 SignalStop();while(InterlockedCompareExchange(&gate,1,0))Sleep(0);
 Close();InterlockedExchange(&gate,0);
}
}
