#include "fixes/code_patch_threads.h"
namespace code_patch_threads {
// Native handle enumeration avoids heap-backed Toolhelp snapshots while a
// suspended thread may own the process heap. ABI: phnt/ntpsapi.h NtGetNextThread.
typedef LONG (NTAPI*NextFn)(HANDLE,HANDLE,ACCESS_MASK,ULONG,ULONG,PHANDLE);
typedef LONG (NTAPI*ResumeFn)(HANDLE,PULONG);
typedef LONG (NTAPI*TerminateFn)(HANDLE,LONG);
static HANDLE emergencyLog=INVALID_HANDLE_VALUE;
bool Prepare(const char*path){
 if(emergencyLog!=INVALID_HANDLE_VALUE)return true;
 if(!path||!*path)return false;
 emergencyLog=CreateFileA(path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
 return emergencyLog!=INVALID_HANDLE_VALUE;
}
#ifndef KEO_PATCH_THREAD_SUSPEND
#define KEO_PATCH_THREAD_SUSPEND(thread) SuspendThread(thread)
#endif
#ifndef KEO_PATCH_THREAD_CONTEXT
#define KEO_PATCH_THREAD_CONTEXT(thread,context) GetThreadContext(thread,context)
#endif
#ifndef KEO_PATCH_THREAD_RESUME
#define KEO_PATCH_THREAD_RESUME(thread) ResumeThread(thread)
#endif
#ifndef KEO_PATCH_THREAD_NATIVE_RESUME
#define KEO_PATCH_THREAD_NATIVE_RESUME(fn,thread) (fn)(thread,NULL)
#endif
static bool Known(const Set*s,DWORD id){
 for(unsigned i=0;i<s->count;++i)if(s->ids[i]==id&&WaitForSingleObject(s->handles[i],0)!=WAIT_OBJECT_0)return true;
 return false;
}
bool Begin(Set*s){
 if(!s||s->count)return false;
 // Resolve before the first suspension: no loader/heap work inside exclusion.
 NextFn next=(NextFn)GetProcAddress(GetModuleHandleA("ntdll.dll"),"NtGetNextThread");
 if(!next)return false;
 s->nativeResume=(void*)GetProcAddress(GetModuleHandleA("ntdll.dll"),"NtResumeThread");
 s->nativeTerminate=(void*)GetProcAddress(GetModuleHandleA("ntdll.dll"),"NtTerminateProcess");
 if(!s->nativeResume||!s->nativeTerminate)return false;
 const HANDLE process=GetCurrentProcess();const DWORD self=GetCurrentThreadId();
 const ACCESS_MASK access=THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION|SYNCHRONIZE;
 for(unsigned pass=0;pass<8;++pass){
  bool added=false,complete=false,ok=true;HANDLE cursor=NULL;
  for(;;){
   HANDLE thread=NULL;const LONG status=next(process,cursor,access,0,0,&thread);
   if(cursor)CloseHandle(cursor);cursor=NULL;
   if((ULONG)status==0x8000001aUL){complete=true;break;}
   if(status<0||!thread){ok=false;break;}
   cursor=thread;const DWORD id=GetThreadId(thread);
   if(!id){ok=false;break;}
   if(id==self||Known(s,id))continue;
   if(s->count==512){ok=false;break;}
   HANDLE held=NULL;
   if(!DuplicateHandle(process,thread,process,&held,0,FALSE,DUPLICATE_SAME_ACCESS)){ok=false;break;}
   if(KEO_PATCH_THREAD_SUSPEND(held)==(DWORD)-1){const bool exited=WaitForSingleObject(held,0)==WAIT_OBJECT_0;CloseHandle(held);if(exited)continue;ok=false;break;}
   s->handles[s->count]=held;s->ids[s->count++]=id;added=true;
   // Suspension completion/CPU context synchronization, not diagnostics.
   CONTEXT context={0};context.ContextFlags=CONTEXT_CONTROL;
   if(!KEO_PATCH_THREAD_CONTEXT(held,&context)&&WaitForSingleObject(held,0)!=WAIT_OBJECT_0){ok=false;break;}
  }
  if(cursor)CloseHandle(cursor);
  if(!ok||!complete)return false;
  // Repeat until no new threads appeared while the previous set was frozen.
  if(!added)return true;
 }
 return false;
}
bool End(Set*s){
 if(!s)return false;unsigned kept=0;const unsigned count=s->count;
 for(unsigned i=0;i<count;++i){HANDLE thread=s->handles[i];bool done=false;
  for(unsigned retry=0;retry<4&&!done;++retry){
   if(WaitForSingleObject(thread,0)==WAIT_OBJECT_0||KEO_PATCH_THREAD_RESUME(thread)!=(DWORD)-1)done=true;
   else if(s->nativeResume&&KEO_PATCH_THREAD_NATIVE_RESUME((ResumeFn)s->nativeResume,thread)>=0)done=true;
  }
  if(done)CloseHandle(thread);
  else{s->handles[kept]=thread;s->ids[kept++]=s->ids[i];}
 }
 for(unsigned i=kept;i<count;++i){s->handles[i]=NULL;s->ids[i]=0;}
 s->count=kept;
 // Never discard a still-live suspension after an OS failure. Caller owns
 // this persistent POD and can retry End without losing the exact handles.
 return kept==0;
}
bool Finish(Set*s){
 for(unsigned retry=0;retry<4;++retry)if(End(s))return true;
 // With outstanding live suspensions, logging/allocating or continuing the
 // engine can deadlock. Every ordinary and native resume path has failed.
 // The handle and terminate entrypoint were prepared before suspension.
 static const char text[]="\r\n[ERROR] KEO code patch: Windows could not release suspended threads after all resume retries; process stopped before unsafe engine continuation.\r\n";
 DWORD written=0;
 HANDLE output=emergencyLog!=INVALID_HANDLE_VALUE?emergencyLog:GetStdHandle(STD_ERROR_HANDLE);
 if(output&&output!=INVALID_HANDLE_VALUE){WriteFile(output,text,sizeof(text)-1,&written,NULL);FlushFileBuffers(output);}
 if(s&&s->nativeTerminate)((TerminateFn)s->nativeTerminate)(GetCurrentProcess(),(LONG)0xe04b454fUL);
 TerminateProcess(GetCurrentProcess(),0xe04b454fUL);
 return false;
}
}
