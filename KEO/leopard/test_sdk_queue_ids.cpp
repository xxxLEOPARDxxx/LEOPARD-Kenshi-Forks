#include <windows.h>
#include <cstdio>
#include <string>
#define KLIB_EXPORT
namespace KenshiLib { enum HookStatus{SUCCESS,FAIL};HookStatus QueueHook(void*,void*,void**);HookStatus ApplyQueuedHooks(); }
KenshiLib::HookStatus QueueHookInternal(size_t,void*,void*,void**);
KenshiLib::HookStatus ApplyQueuedHooksInternal(size_t);
static size_t queuedId=0,appliedId=0;
static BOOL SpyModule(DWORD,LPCSTR p,HMODULE*out){*out=(HMODULE)(p==(LPCSTR)&QueueHookInternal?0x22220000:0x11110000);return TRUE;}
#define GetModuleHandleExA SpyModule
static void ErrorLog(const std::string&){}
// SDK_STATIC_WRAPPERS
KenshiLib::HookStatus QueueHookInternal(size_t id,void*,void*,void**o){queuedId=id;*o=(void*)0x1234;return KenshiLib::SUCCESS;}
KenshiLib::HookStatus ApplyQueuedHooksInternal(size_t id){appliedId=id;return KenshiLib::SUCCESS;}
int main(){void*original=NULL;KenshiLib::QueueHook((void*)1,(void*)2,&original);KenshiLib::ApplyQueuedHooks();if(queuedId!=appliedId){std::printf("EXPECTED RED: actual SDK wrappers queue id=%Ix apply id=%Ix\n",queuedId,appliedId);return 1;}return 0;}
