#include "fixes/world/save_read_buffer.h"
#include "fixes/world/save_gog_native.h"
#include "plugin/hook_manifest.h"
#include <windows.h>
#include <intrin.h>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <io.h>
#ifndef KEO_SAVE_READ_TEST
#include "base/core.h"
#include "base/log_admission.h"
#include "fixes/fixes_config.h"
#include "render/module_hooks.h"
#include "base/klib_include.h"
#include <core/Functions.h>
#include "base/klib_include_end.h"
#endif
namespace save_read_buffer {
typedef void*(*CtorFn)(void*,const wchar_t*,int,int,int);
static CtorFn original=NULL;
static bool tried=false,admitted=false;
static uintptr_t gameBase=0;
struct Layout {uintptr_t constructor,caller,filebuf,setbuf;};
static const Layout steamLayout={0x6c8330,0x6c092c,0x168ef18,0x13a20};
static const Layout gogLayout={0x6c7c40,0x6c023c,0x168df18,0x13a1b};
static const Layout* layout=&steamLayout;
static volatile LONG errorLogged=0;
static_assert(sizeof(std::ifstream)==0x110&&sizeof(std::filebuf)==0xa0,"native VC10 input stream ABI");
static void Error(){if(InterlockedCompareExchange(&errorLogged,1,0)==0){try{LogError("Save read buffering: native buffer request failed; normal reading retained");}catch(...){}}}
static bool SavePath(const wchar_t*path){
 if(!path)return false;
 const wchar_t*extension=NULL;size_t i=0;
 for(;i<32768&&path[i];++i){if(path[i]==L'/'||path[i]==L'\\')extension=NULL;else if(path[i]==L'.')extension=path+i;}
 return i<32768&&extension&&(!_wcsicmp(extension,L".save")||!_wcsicmp(extension,L".platoon")||!_wcsicmp(extension,L".zone"));
}
#ifndef KEO_SAVE_SETBUF
#define KEO_SAVE_SETBUF(buffer,size) (buffer)->pubsetbuf(NULL,size)
#endif
#ifndef KEO_SAVE_FILE_SIZE
#define KEO_SAVE_FILE_SIZE(handle,size) GetFileSizeEx(handle,size)
#endif
static void Prepare(void*self){
 if(!self)return;
 // Exact fresh native ifstream. Reject foreign filebuf implementations before
 // using the VC10 view; no FILE or stream pointer survives this call.
 void*buffer=static_cast<char*>(self)+0x10;
 uintptr_t table=0;memcpy(&table,buffer,sizeof(table));
 if(table!=gameBase+layout->filebuf)return;
 void*codecvt=NULL;FILE*file=NULL;
 memcpy(&codecvt,static_cast<char*>(buffer)+0x80,sizeof(codecvt));
 memcpy(&file,static_cast<char*>(buffer)+0x98,sizeof(file));
 // filebuf::setbuf resets conversion state. A binary stream can still have
 // a nontrivial locale facet; leave those streams entirely native.
 if(codecvt||!file||file->_base||file->_ptr||file->_cnt||file->_flag!=_IOREAD||file->_file<0)return;
 std::ifstream*stream=static_cast<std::ifstream*>(self);
 if(!stream->good()||stream->gcount()!=0||stream->rdbuf()!=buffer)return;
 // Tiny platoons do not benefit from a large read buffer. Query the existing
 // handle without opening/reading/seeking or changing native FILE position.
 HANDLE handle=(HANDLE)_get_osfhandle(file->_file);LARGE_INTEGER size;
 if(handle==INVALID_HANDLE_VALUE||!KEO_SAVE_FILE_SIZE(handle,&size)||size.QuadPart<128*1024)return;
 try{
  // CRT owns this buffer until normal close. On allocation failure it may
  // remove buffering; the fresh-empty FILE gate above excludes stale pointers.
  if(!KEO_SAVE_SETBUF(stream->rdbuf(),65536))Error();
 }catch(...){Error();}
}
#ifndef KEO_SAVE_CALLER
#define KEO_SAVE_CALLER() (uintptr_t)_ReturnAddress()
#endif
static __declspec(noinline) void*Construct(void*self,const wchar_t*path,int mode,int protection,int mostDerived){
 const uintptr_t caller=KEO_SAVE_CALLER();
 // Native exceptions and return value remain native; optional preparation is
 // only after a successful exact constructor return and before the first read.
 void*result=original(self,path,mode,protection,mostDerived);
 if(result==self&&self&&IsMainThread()&&admitted&&fixes::g_fixesCfg.saveReadBufferEnabled
    &&caller==gameBase+layout->caller&&mode==0x20&&protection==0x40&&mostDerived==1&&SavePath(path))Prepare(self);
 return result;
}
struct Proof{uintptr_t rva;const unsigned char*bytes;size_t size;};
// Filled from the pinned on-disk image; no signature scanning or guessed SDK RVA.
static const unsigned char proof0[]={0x48,0x89,0x4c,0x24,0x08,0x56,0x57,0x41,0x54,0x48,0x83,0xec,0x30,0x48,0xc7,0x44,0x24,0x20,0xfe,0xff,0xff,0xff,0x48,0x89,0x5c,0x24,0x58,0x48,0x89,0x6c,0x24,0x68};
static const unsigned char proof1[]={0xc7,0x44,0x24,0x20,0x01,0x00,0x00,0x00,0x41,0xb9,0x40,0x00,0x00,0x00,0x45,0x8d,0x41,0xe0,0x48,0x8d,0x4d,0xf0,0xe8,0x69,0x82,0x97,0xff};
static const unsigned char proof2[]={0xe9,0x96,0xf7,0x68,0x00};
static const unsigned char proof3[]={0x48,0x8d,0x05,0xfe,0x7d,0xfc,0x00,0x48,0x89,0x01,0x48,0x81,0xc1,0xb0,0x00,0x00,0x00};
static const unsigned char proof4[]={0x48,0x8d,0x5f,0x10,0x45,0x33,0xc9,0x45,0x33,0xc0,0x48,0x8b,0xd3,0x48,0x8b,0xcf,0xff,0x15,0xb7,0xbf,0xb7,0x01};
static const unsigned char proof5[]={0x48,0x8d,0x05,0x4e,0x6b,0xfc,0x00,0x48,0x89,0x03,0xc6,0x83,0x90,0x00,0x00,0x00,0x00,0xc6,0x83,0x89,0x00,0x00,0x00,0x00,0x48,0x8b,0xcb,0xff,0x15,0xd4,0xbf,0xb7,0x01,0x48,0xc7,0x83,0x98,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x8b,0x05,0x97,0xef,0xa5,0x01,0x89,0x83,0x8c,0x00,0x00,0x00,0x48,0xc7,0x83,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static const unsigned char proof6[]={0xe9,0x9b,0xe9,0x11,0x00};
static const unsigned char proof7[]={0x40,0x53,0x48,0x83,0xec,0x40,0x48,0x8b,0xd9,0x48,0x8b,0x89,0x98,0x00,0x00,0x00,0x4d,0x8b,0xc8,0x48,0x85,0xc9,0x0f,0x84,0x93,0x00,0x00,0x00,0x48,0x85,0xd2,0x75,0x0b,0x4d,0x85,0xc0,0x75,0x06,0x45,0x8d,0x41,0x04,0xeb,0x03,0x45,0x33,0xc0,0xff,0x15,0x9b,0x2b,0x11,0x02,0x85,0xc0,0x75,0x76,0x48,0x8b,0xcb,0x48,0x89,0x7c,0x24,0x50,0x48,0x8b,0xbb,0x98,0x00,0x00,0x00,0xc6,0x83,0x90,0x00,0x00,0x00,0x01,0x88,0x83,0x89,0x00,0x00,0x00,0xff,0x15,0x9d,0x1f,0x11,0x02,0x48,0x85,0xff,0x74,0x23,0x4c,0x8d,0x4f,0x08,0x48,0x8d,0x57,0x10,0x4c,0x8b,0xc7,0x4c,0x89,0x4c,0x24,0x30,0x48,0x8b,0xcb,0x48,0x89,0x7c,0x24,0x28,0x48,0x89,0x54,0x24,0x20,0xff,0x15,0x7d,0x1f,0x11,0x02,0x48,0x89,0xbb,0x98,0x00,0x00,0x00,0x8b,0x05,0x3c,0x4f,0xff,0x01,0x48,0x8b,0x7c,0x24,0x50,0x89,0x83,0x8c,0x00,0x00,0x00,0x48,0xc7,0x83,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x48,0x8b,0xc3,0x48,0x83,0xc4,0x40,0x5b,0xc3,0x33,0xc0,0x48,0x83,0xc4,0x40,0x5b,0xc3,0xcc,0xcc,0xcc};
static const Proof proofs[]={{0x6c8330,proof0,sizeof(proof0)},{0x6c0911,proof1,sizeof(proof1)},{0x38b95,proof2,sizeof(proof2)},{0x6c836b,proof3,sizeof(proof3)},{0x6c838b,proof4,sizeof(proof4)},{0x6c83c3,proof5,sizeof(proof5)},{0x13a20,proof6,sizeof(proof6)},{0x1323c0,proof7,sizeof(proof7)}};
static const save_gog_native::Window gogProofs[]={
 {0x6c7c40,32,0xb2deec046e63d263ULL}, // Steam 0x6c8330
 {0x6c0221,27,0x1c8a5232ade8b1fbULL}, // Steam 0x6c0911
 {0x38bfe,5,0x2cb1bc30969ff3c1ULL}, // Steam 0x38b95
 {0x6c7c7b,17,0xcaf4b70414300a57ULL}, // Steam 0x6c836b
 {0x6c7c9b,22,0x1375fc861228673dULL}, // Steam 0x6c838b
 {0x6c7cd3,67,0x73d2a5bd47ab1f87ULL}, // Steam 0x6c83c3
 {0x13a1b,5,0x52c9cc76e47d7665ULL}, // Steam 0x13a20
 {0x1322e0,186,0x6cdd860e80fa384cULL}, // Steam 0x1323c0
};
static bool Admit(HMODULE module){
 __try{DWORD stamp=0,size=0;
  if(!module||!ReadModuleImageId(module,&stamp,&size))return false;
  const bool gog=save_gog_native::IsGogImage(stamp,size);
  if(gog){
   if(!save_gog_native::Match(module,gogProofs))return false;
  }else{
   if(stamp!=0x65d604d7||size!=0x232c000)return false;
   for(size_t i=0;i<sizeof(proofs)/sizeof(proofs[0]);++i)
    if(memcmp((char*)module+proofs[i].rva,proofs[i].bytes,proofs[i].size))return false;
  }
  const Layout* current=gog?&gogLayout:&steamLayout;
  uintptr_t setbuf=0;memcpy(&setbuf,(char*)module+current->filebuf+0x60,sizeof(setbuf));
  if(setbuf!=(uintptr_t)module+current->setbuf)return false;
  layout=current;return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}

static void CompleteInstall(int,const char* why){
 if(why||!original){
  LogError(std::string("Save read buffering: constructor hook failed (")+(why?why:"original")+"); normal reading retained");
  return;
 }
 admitted=true;
 KEO_DETAIL("Save read buffering: exact fresh native save streams admitted");
}

void Install(int*,int*){
 if(tried)return;tried=true;
#ifndef KEO_SAVE_MODULE
#define KEO_SAVE_MODULE() GetModuleHandleA(NULL)
#endif
 HMODULE game=KEO_SAVE_MODULE();
 if(!Admit(game)){LogError("Save read buffering: native stream contract refused; normal reading retained");return;}
 gameBase=(uintptr_t)game;
 void* target=(char*)game+layout->constructor;
 if(QueueStartupAuxHook(STARTUP_AUX_SAVE_READ_BUFFER,target,(void*)&Construct,(void**)&original,CompleteInstall))return;
 const bool installed=KenshiLib::AddHook(target,(void*)&Construct,(void**)&original)==KenshiLib::SUCCESS;
 CompleteInstall(STARTUP_AUX_SAVE_READ_BUFFER,installed?NULL:"hook");
}
}
void InstallSaveReadBuffer(int*installed,int*total){save_read_buffer::Install(installed,total);}
