// Offline VC10 file streams + production wrapper. No game/process execution.
#include <windows.h>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <stdexcept>
#include <locale>
#include <codecvt>
#include "plugin/hook_manifest.h"
static unsigned checks=0,errors=0,nativeCalls=0,setbufCalls=0;
static bool owner=true,logThrows=false,nativeThrows=false,alternateReturn=false,preRead=false,closeEarly=false,wrongTable=false;
static bool failSize=false;static int fault=0,seenMode=0,seenProtection=0,seenDerived=0;static uintptr_t suppliedCaller=0;static std::streamsize requested=0;
static void*seenSelf=NULL;static const wchar_t*seenPath=NULL;
bool IsMainThread(){return owner;}
static void LogError(const std::string&){++errors;if(logThrows)throw std::runtime_error("logger");}
namespace fixes {struct Config{bool saveReadBufferEnabled;};Config g_fixesCfg={true};}
#define KEO_DETAIL(x) ((void)0)
static HMODULE module=NULL;static unsigned hooks=0;static int hookResult=0;static bool nullOriginal=false;
static bool queueStartup=false;
static unsigned queued=0;
static StartupAuxHookCompletion queuedDone=NULL;
static void** queuedOriginal=NULL;
bool QueueStartupAuxHook(StartupAuxHookId id,void*,void*,void** original,StartupAuxHookCompletion done){
 if(!queueStartup||id!=STARTUP_AUX_SAVE_READ_BUFFER)return false;
 ++queued;queuedDone=done;queuedOriginal=original;return true;
}
namespace KenshiLib {enum Status{SUCCESS,FAILURE};static Status AddHook(void*,void*,void**out){++hooks;*out=nullOriginal?NULL:(void*)0x12345;return (Status)hookResult;}}
bool ReadModuleImageId(HMODULE m,DWORD*stamp,DWORD*size){const IMAGE_DOS_HEADER*d=(const IMAGE_DOS_HEADER*)m;if(d->e_magic!=IMAGE_DOS_SIGNATURE||d->e_lfanew<0||d->e_lfanew>1024*1024)return false;const IMAGE_NT_HEADERS64*n=(const IMAGE_NT_HEADERS64*)((char*)m+d->e_lfanew);if(n->Signature!=IMAGE_NT_SIGNATURE||n->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64)return false;*stamp=n->FileHeader.TimeDateStamp;*size=n->OptionalHeader.SizeOfImage;return true;}
static std::streambuf*Setbuf(std::filebuf*b,std::streamsize n){++setbufCalls;requested=n;if(fault==1){b->pubsetbuf(NULL,0);return NULL;}if(fault==2)throw std::bad_alloc();return b->pubsetbuf(NULL,n);}
#define KEO_SAVE_FILE_SIZE(handle,size) (failSize?FALSE:GetFileSizeEx(handle,size))
#define KEO_SAVE_READ_TEST
#define KEO_SAVE_MODULE() module
#define KEO_SAVE_CALLER() suppliedCaller
#define KEO_SAVE_SETBUF(buffer,size) Setbuf(buffer,size)
#ifndef KEO_SAVE_READ_SOURCE
#define KEO_SAVE_READ_SOURCE "../src/fixes/world/save_read_buffer.cpp"
#endif
#include KEO_SAVE_READ_SOURCE
#include "save_gog_test_fixture.h"
#define C(x) do{++checks;if(!(x)){printf("CHECK line%d %s\n",__LINE__,#x);return 1;}}while(0)
typedef std::ifstream Stream;
struct Storage {__declspec(align(8)) unsigned char bytes[sizeof(Stream)];};
static void*Native(void*self,const wchar_t*path,int mode,int protection,int derived){
 ++nativeCalls;seenSelf=self;seenPath=path;seenMode=mode;seenProtection=protection;seenDerived=derived;
 if(nativeThrows)throw std::runtime_error("native constructor");
 Stream*in=new(self) Stream(path,std::ios::binary,protection);
 if(preRead){char ch;in->read(&ch,1);}if(closeEarly)in->close();if(wrongTable){++save_read_buffer::gameBase;++suppliedCaller;}
 return alternateReturn?(char*)self+1:self;
}
static const wchar_t*path=L"build/save_read_buffer_test/pattern.save";
static void Defaults(){owner=true;logThrows=nativeThrows=alternateReturn=preRead=closeEarly=wrongTable=false;fault=0;failSize=false;nativeCalls=setbufCalls=0;fixes::g_fixesCfg.saveReadBufferEnabled=true;save_read_buffer::admitted=true;
 Stream in;uintptr_t table=0;memcpy(&table,(char*)in.rdbuf(),8);save_read_buffer::gameBase=table-0x168ef18;suppliedCaller=save_read_buffer::gameBase+0x6c092c;save_read_buffer::original=Native;
}
static int Admission(){
 SaveGogTestImage gog;
 if(gog.requested){
  C(gog.bytes);uintptr_t*slot=(uintptr_t*)(gog.bytes+0x168df78);
  *slot=(uintptr_t)gog.bytes+0x13a1b;
  C(save_read_buffer::Admit(gog.Module())&&save_read_buffer::layout->constructor==0x6c7c40&&save_read_buffer::layout->caller==0x6c023c);
  for(size_t i=0;i<sizeof(save_read_buffer::gogProofs)/sizeof(save_read_buffer::gogProofs[0]);++i){
   unsigned char&byte=gog.bytes[save_read_buffer::gogProofs[i].rva];byte^=1;C(!save_read_buffer::Admit(gog.Module()));byte^=1;
  }
  ++*slot;C(!save_read_buffer::Admit(gog.Module()));
 }
 module=(HMODULE)VirtualAlloc(NULL,0x232c000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);C(module!=NULL);
 IMAGE_DOS_HEADER*d=(IMAGE_DOS_HEADER*)module;d->e_magic=IMAGE_DOS_SIGNATURE;d->e_lfanew=0x100;IMAGE_NT_HEADERS64*n=(IMAGE_NT_HEADERS64*)((char*)module+0x100);n->Signature=IMAGE_NT_SIGNATURE;n->FileHeader.Machine=IMAGE_FILE_MACHINE_AMD64;n->FileHeader.TimeDateStamp=0x65d604d7;n->OptionalHeader.SizeOfImage=0x232c000;
 for(size_t i=0;i<sizeof(save_read_buffer::proofs)/sizeof(save_read_buffer::proofs[0]);++i){const save_read_buffer::Proof&p=save_read_buffer::proofs[i];memcpy((char*)module+p.rva,p.bytes,p.size);}
 uintptr_t address=(uintptr_t)module+0x13a20;memcpy((char*)module+0x168ef78,&address,8);C(save_read_buffer::Admit(module));C(!save_read_buffer::Admit(NULL));C(!save_read_buffer::Admit((HMODULE)1));
 for(size_t i=0;i<sizeof(save_read_buffer::proofs)/sizeof(save_read_buffer::proofs[0]);++i){char*p=(char*)module+save_read_buffer::proofs[i].rva;*p^=1;C(!save_read_buffer::Admit(module));*p^=1;}
 n->FileHeader.TimeDateStamp^=1;C(!save_read_buffer::Admit(module));n->FileHeader.TimeDateStamp^=1;n->OptionalHeader.SizeOfImage^=1;C(!save_read_buffer::Admit(module));n->OptionalHeader.SizeOfImage^=1;
 ++address;memcpy((char*)module+0x168ef78,&address,8);C(!save_read_buffer::Admit(module));--address;memcpy((char*)module+0x168ef78,&address,8);
 for(unsigned f=0;f<4;++f){save_read_buffer::tried=save_read_buffer::admitted=false;hooks=0;hookResult=f==1?1:0;nullOriginal=f==2;save_read_buffer::original=NULL;if(f==3)n->FileHeader.TimeDateStamp^=1;
  save_read_buffer::Install(NULL,NULL);C(save_read_buffer::tried);C(save_read_buffer::admitted==(f==0));C(hooks==(f==3?0u:1u));if(f==1)C(save_read_buffer::original==(save_read_buffer::CtorFn)0x12345);save_read_buffer::Install(NULL,NULL);C(hooks==(f==3?0u:1u));if(f==3)n->FileHeader.TimeDateStamp^=1;
 }
 queueStartup=true;
 for(unsigned mode=0;mode<3;++mode){
  save_read_buffer::tried=save_read_buffer::admitted=false;save_read_buffer::original=NULL;queued=hooks=0;
  const unsigned before=errors;
  save_read_buffer::Install(NULL,NULL);
  C(queued==1&&hooks==0&&!save_read_buffer::admitted);
  if(mode!=2)*queuedOriginal=reinterpret_cast<void*>(&Native);
  queuedDone(STARTUP_AUX_SAVE_READ_BUFFER,mode==1?"batch-apply":NULL);
  C(save_read_buffer::admitted==(mode==0)&&errors==before+(mode==0?0u:1u));
  C(save_read_buffer::original==(mode==2?NULL:Native));
  save_read_buffer::Install(NULL,NULL);
  C(queued==1&&hooks==0);
 }
 queueStartup=false;
 VirtualFree(module,0,MEM_RELEASE);module=NULL;return 0;
}
static std::vector<char>data;
static int Reads(){
 for(unsigned enabled=0;enabled<2;++enabled){Defaults();fixes::g_fixesCfg.saveReadBufferEnabled=enabled!=0;Storage storage;C(save_read_buffer::Construct(storage.bytes,path,0x20,0x40,1)==storage.bytes);Stream*in=(Stream*)storage.bytes;C(nativeCalls==1&&setbufCalls==enabled);char first=0;in->read(&first,1);C(first==data[0]&&in->gcount()==1);C(in->rdbuf()->in_avail()==(enabled?65535:4095));
  in->seekg(0);std::vector<char>copy(data.size());unsigned refills=0;for(size_t i=0;i<copy.size();++i){std::streamsize before=in->rdbuf()->in_avail();in->read(&copy[i],1);if(in->rdbuf()->in_avail()>before)++refills;C(in->gcount()==1&&in->good());}C(copy==data);printf("buffer %u: %u refills for %u single-byte reads\n",enabled?65536:4096,refills,(unsigned)data.size());C(refills==(enabled?4u:50u));
  char tail[8]={};in->read(tail,8);C(in->gcount()==0&&in->eof()&&in->fail());in->clear();in->seekg(-3,std::ios::end);in->read(tail,8);C(in->gcount()==3&&in->eof()&&in->fail());C(!memcmp(tail,&data[data.size()-3],3));in->clear();in->seekg(65533);in->read(tail,8);C(in->gcount()==8&&!memcmp(tail,&data[65533],8));in->close();C(!in->is_open());in->~Stream();
 }
 return 0;
}
static int Gates(){
 C(save_read_buffer::SavePath(L"a.SAVE"));C(save_read_buffer::SavePath(L"a.platoon"));C(save_read_buffer::SavePath(L"a.zone"));C(!save_read_buffer::SavePath(L"a.mod"));C(!save_read_buffer::SavePath(NULL));C(!save_read_buffer::SavePath(L"folder.save/no-extension"));
 for(unsigned kind=0;kind<12;++kind){Defaults();int mode=0x20,protection=0x40,derived=1;const wchar_t*p=path;
  if(kind==0)owner=false;if(kind==1)save_read_buffer::admitted=false;if(kind==2)++suppliedCaller;if(kind==3)mode=0x21;if(kind==4)protection=0x20;if(kind==5)derived=0;if(kind==6)p=L"build/save_read_buffer_test/missing.save";if(kind==7)preRead=true;if(kind==8)closeEarly=true;if(kind==9)wrongTable=true;if(kind==10)alternateReturn=true;if(kind==11)p=L"build/save_read_buffer_test/pattern.mod";
  Storage storage;void*r=save_read_buffer::Construct(storage.bytes,p,mode,protection,derived);Stream*in=(Stream*)storage.bytes;C(nativeCalls==1&&setbufCalls==0);C(r==(kind==10?(void*)((char*)storage.bytes+1):(void*)storage.bytes));C(seenSelf==storage.bytes&&seenPath==p&&seenMode==mode&&seenProtection==protection&&seenDerived==derived);in->~Stream();
 }
 Defaults();{Stream foreign(path,std::ios::binary);++save_read_buffer::gameBase;save_read_buffer::Prepare(&foreign);C(setbufCalls==0);}
 Defaults();{Stream invalid(path,std::ios::binary);FILE*f=NULL;memcpy(&f,(char*)invalid.rdbuf()+0x98,8);int fd=f->_file;f->_file=-1;save_read_buffer::Prepare(&invalid);f->_file=fd;C(setbufCalls==0);}
 Defaults();nativeThrows=true;Storage storage;bool caught=false;try{save_read_buffer::Construct(storage.bytes,path,0x20,0x40,1);}catch(const std::runtime_error&e){caught=std::string(e.what())=="native constructor";}C(caught&&nativeCalls==1&&setbufCalls==0);
 return 0;
}
class Conversion:public std::codecvt<char,char,std::mbstate_t> {
protected:
 bool do_always_noconv()const throw(){return false;}
 result do_in(std::mbstate_t&,const char*first,const char*last,const char*&next,char*out,char*outLast,char*&outNext)const {next=first;outNext=out;while(next!=last&&outNext!=outLast)*outNext++=*next++^0x5a;return next==last?ok:partial;}
 int do_encoding()const throw(){return 1;}
};
static int SizeGate(){
 const wchar_t*names[]={L"build/save_read_buffer_test/empty.save",L"build/save_read_buffer_test/tiny.platoon"};
 for(unsigned i=0;i<2;++i){Defaults();{std::ofstream out(names[i],std::ios::binary);if(i)out.write(&data[0],100);}Storage storage;save_read_buffer::Construct(storage.bytes,names[i],0x20,0x40,1);Stream*in=(Stream*)storage.bytes;C(nativeCalls==1&&setbufCalls==0);char bytes[101]={};in->read(bytes,101);C(in->gcount()==(i?100:0)&&in->eof());if(i)C(!memcmp(bytes,&data[0],100));in->~Stream();}
 Defaults();failSize=true;Storage storage;save_read_buffer::Construct(storage.bytes,path,0x20,0x40,1);Stream*in=(Stream*)storage.bytes;C(setbufCalls==0);char first=0;in->read(&first,1);C(first==data[0]&&in->gcount()==1);in->~Stream();return 0;
}
static int ConversionGate(){Defaults();std::locale old=std::locale::global(std::locale(std::locale::classic(),new Conversion));Storage storage;save_read_buffer::Construct(storage.bytes,path,0x20,0x40,1);std::locale::global(old);Stream*in=(Stream*)storage.bytes;C(setbufCalls==0);char ch;in->read(&ch,1);C(ch==(data[0]^0x5a));in->~Stream();return 0;}
static int Failures(){for(fault=1;fault<=2;){int f=fault;Defaults();fault=f;save_read_buffer::errorLogged=0;unsigned before=errors;logThrows=f==2;Storage storage;save_read_buffer::Construct(storage.bytes,path,0x20,0x40,1);Stream*in=(Stream*)storage.bytes;C(nativeCalls==1&&setbufCalls==1&&errors==before+1);std::vector<char>copy(data.size());in->read(&copy[0],copy.size());C(in->gcount()==copy.size()&&copy==data&&in->good());in->~Stream();fault=f+1;}fault=0;return 0;}
int main(){CreateDirectoryA("build",NULL);CreateDirectoryA("build/save_read_buffer_test",NULL);data.resize(200003);for(size_t i=0;i<data.size();++i)data[i]=(char)((i*37+i/511)&255);{std::ofstream out(path,std::ios::binary);out.write(&data[0],data.size());}{std::ofstream out(L"build/save_read_buffer_test/pattern.mod",std::ios::binary);out.write(&data[0],data.size());}
 C(Admission()==0);C(Reads()==0);C(Gates()==0);C(ConversionGate()==0);C(SizeGate()==0);C(Failures()==0);printf("PASS %u actual VC10 native-stream/wrapper checks\n",checks);return 0;
}
