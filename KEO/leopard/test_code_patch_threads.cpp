// Isolated process only. No game/process instrumentation.
#include <windows.h>
#include <cstdio>
static int suspendFault=-1,contextFault=-1,suspends=0,contexts=0;
static bool resumeFault=false,nativeResumeFault=false;
static DWORD Suspend(HANDLE h){const int n=suspends++;return n==suspendFault?(DWORD)-1:SuspendThread(h);}
static BOOL Context(HANDLE h,CONTEXT*c){const int n=contexts++;return n==contextFault?FALSE:GetThreadContext(h,c);}
static DWORD Resume(HANDLE h){return resumeFault?(DWORD)-1:ResumeThread(h);}
typedef LONG(NTAPI*ResumeFn)(HANDLE,PULONG);
static LONG NativeResume(ResumeFn fn,HANDLE h){return nativeResumeFault?(LONG)0xc0000001UL:fn(h,NULL);}
#ifdef KEO_THREAD_EXCLUSION_BASELINE
namespace code_patch_threads{struct Set{unsigned count;};bool Begin(Set*s){s->count=0;return true;}bool End(Set*){return true;}}
#else
#define KEO_PATCH_THREAD_SUSPEND(thread) Suspend(thread)
#define KEO_PATCH_THREAD_CONTEXT(thread,context) Context(thread,context)
#define KEO_PATCH_THREAD_RESUME(thread) Resume(thread)
#define KEO_PATCH_THREAD_NATIVE_RESUME(fn,thread) NativeResume(fn,thread)
#include "../src/fixes/code_patch_threads.cpp"
#endif
static volatile LONG ticks=0,stop=0,ready=0;
static DWORD WINAPI Worker(void*){InterlockedIncrement(&ready);while(!InterlockedCompareExchange(&stop,0,0)){InterlockedIncrement(&ticks);SwitchToThread();}return 0;}
int main(int argc,char**){HANDLE thread=CreateThread(NULL,0,Worker,NULL,0,NULL);if(!thread)return 2;while(!InterlockedCompareExchange(&ready,0,0))Sleep(1);
#ifndef KEO_THREAD_EXCLUSION_BASELINE
 if(argc>1){code_patch_threads::Set fatal={0};if(!code_patch_threads::Prepare("build/patch_thread_test/critical.log")||!code_patch_threads::Begin(&fatal))return 2;resumeFault=nativeResumeFault=true;code_patch_threads::Finish(&fatal);return 3;}
#endif
 code_patch_threads::Set set={0};bool began=code_patch_threads::Begin(&set);LONG a=InterlockedCompareExchange(&ticks,0,0);Sleep(10);LONG b=InterlockedCompareExchange(&ticks,0,0);bool ended=code_patch_threads::End(&set);
 bool ok=began&&ended&&a==b;
#ifndef KEO_THREAD_EXCLUSION_BASELINE
 HANDLE second=CreateThread(NULL,0,Worker,NULL,0,NULL);if(!second)return 2;while(InterlockedCompareExchange(&ready,0,0)!=2)Sleep(1);
 suspends=contexts=0;suspendFault=1;began=code_patch_threads::Begin(&set);const unsigned partial=set.count;ended=code_patch_threads::End(&set);ok=ok&&!began&&partial==1&&ended&&set.count==0;suspendFault=-1;
 suspends=contexts=0;contextFault=0;began=code_patch_threads::Begin(&set);const unsigned partialContext=set.count;ended=code_patch_threads::End(&set);ok=ok&&!began&&partialContext==1&&ended&&set.count==0;contextFault=-1;
 const DWORD pre= SuspendThread(thread);began=code_patch_threads::Begin(&set);ended=code_patch_threads::End(&set);const DWORD preserved=ResumeThread(thread);ok=ok&&pre==0&&began&&ended&&preserved==1;
 began=code_patch_threads::Begin(&set);const bool nested=code_patch_threads::Begin(&set);ended=code_patch_threads::End(&set);ok=ok&&began&&!nested&&ended;
 began=code_patch_threads::Begin(&set);resumeFault=true;ended=code_patch_threads::End(&set);resumeFault=false;ok=ok&&began&&ended&&set.count==0;
 began=code_patch_threads::Begin(&set);resumeFault=nativeResumeFault=true;const bool failed=code_patch_threads::End(&set);const unsigned retained=set.count;bool handlesValid=true;for(unsigned i=0;i<set.count;++i){DWORD flags=0;handlesValid=handlesValid&&GetHandleInformation(set.handles[i],&flags)!=FALSE;}resumeFault=nativeResumeFault=false;ended=code_patch_threads::End(&set);ok=ok&&began&&!failed&&retained==2&&handlesValid&&ended&&set.count==0;
#endif
 InterlockedExchange(&stop,1);WaitForSingleObject(thread,5000);
#ifndef KEO_THREAD_EXCLUSION_BASELINE
 WaitForSingleObject(second,5000);CloseHandle(second);
 // Synthetic reused id: a dead retained handle must not mask a live id.
 set.handles[0]=thread;set.ids[0]=GetCurrentThreadId();set.count=1;ok=ok&&!code_patch_threads::Known(&set,GetCurrentThreadId());ended=code_patch_threads::End(&set);ok=ok&&ended&&set.count==0;
#else
 CloseHandle(thread);
#endif
 if(!ok){printf("CHECK own workers must quiesce, resume and preserve suspension ownership (%ld -> %ld)\n",a,b);return 1;}
 puts("PASS isolated exclusion, partial failure, presuspended/nested counts, native resume fallback, retained failed handles and exited id reuse");return 0;
}
