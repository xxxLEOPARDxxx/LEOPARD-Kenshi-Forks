#include <cstdio>
#include <cstring>
#include <string>
#include <stdint.h>
typedef void(*NativeFn)(void*);
static void Native(void*){}
static unsigned char image[64]={};
static void*GetModuleHandleA(const char*){return image;}
static int errors=0,hookCalls=0,status=0;
static void*provided=NULL;
static void LogError(const std::string&){++errors;}
namespace KenshiLib {
enum{SUCCESS=0};
static int AddHook(void*,void*,void**out){++hookCalls;*out=provided;return status;}
}
#include "resource_probe_admission_actual.inc"
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::printf("CHECK line%d %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
 for(int helper=0;helper<2;++helper)for(int mode=0;mode<4;++mode){
  status=mode&1;provided=mode&2?NULL:(void*)&Native;hookCalls=errors=0;
  void*out=(void*)0x1234;bool ok=false;
  if(helper==0)ok=InstallAdmitted("test",image,(void*)&Native,&out);
  else ok=Install("test",0,image,&Native,reinterpret_cast<NativeFn*>(&out));
  CHECK(hookCalls==1);CHECK(ok==(status==0&&provided!=NULL));
  CHECK(out==provided);CHECK(errors==(ok?0:1));
 }
 void*out=(void*)&Native;hookCalls=0;
 CHECK(!InstallAdmitted("missing",NULL,(void*)&Native,&out));CHECK(out==(void*)&Native&&hookCalls==0);
 unsigned char wrong[32]={1};NativeFn fn=&Native;
 CHECK(!Install("wrong",0,wrong,&Native,&fn));CHECK(fn==&Native&&hookCalls==0);
 std::printf("PASS %u actual probe installation checks\n",checks);return 0;
}
