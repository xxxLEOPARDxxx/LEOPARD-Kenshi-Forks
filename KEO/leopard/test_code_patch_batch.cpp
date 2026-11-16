#include <windows.h>
#include <cstdio>
#include <cstring>
static unsigned protects=0,flushes=0,writes=0;
static int protectFault=-1,flushFault=-1,writeFault=-1;
static BOOL Protect(void*p,size_t n,DWORD f,DWORD*old){return (int)protects++==protectFault?FALSE:VirtualProtect(p,n,f,old);}
static BOOL Flush(void*p,size_t n){return (int)flushes++==flushFault?FALSE:FlushInstructionCache(GetCurrentProcess(),p,n);}
static void Write(void*p,const void*data,size_t n){if((int)writes++==writeFault){memcpy(p,data,n>1?2:1);writeFault=-1;RaiseException(0xe9876543,0,0,NULL);}memcpy(p,data,n);}
#define KEO_BATCH_PROTECT(p,n,f,old) Protect(p,n,f,old)
#define KEO_BATCH_FLUSH(p,n) Flush(p,n)
#define KEO_BATCH_WRITE(p,data,n) Write(p,data,n)
#include "../src/fixes/code_patch_batch.cpp"
static unsigned checks=0;
#define C(x) do{++checks;if(!(x)){printf("CHECK line%d %s\n",__LINE__,#x);return 1;}}while(0)
int main(){unsigned char*memory=(unsigned char*)VirtualAlloc(NULL,8192,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);C(memory!=NULL);memset(memory,0x90,8192);DWORD ignored=0;C(VirtualProtect(memory,8192,PAGE_EXECUTE_READ,&ignored)!=FALSE);
 code_patch_batch::Edit edits[3]={};const unsigned offsets[]={5,97,4101};for(unsigned i=0;i<3;++i){edits[i].address=memory+offsets[i];edits[i].size=4;memset(edits[i].before,0x90,4);memset(edits[i].after,0xcc-i,4);}
 code_patch_threads::Set threads={0};bool possible=false;C(code_patch_batch::Apply(edits,3,&threads,&possible));C(possible&&threads.count==0);for(unsigned i=0;i<3;++i)C(memcmp(edits[i].address,edits[i].after,4)==0);
 MEMORY_BASIC_INFORMATION mbi;C(VirtualQuery(memory,&mbi,sizeof(mbi))==sizeof(mbi)&&mbi.Protect==PAGE_EXECUTE_READ);C(VirtualQuery(memory+4096,&mbi,sizeof(mbi))==sizeof(mbi)&&mbi.Protect==PAGE_EXECUTE_READ);
 // Restore data between cases, outside any executable native function.
 for(unsigned mode=0;mode<13;++mode){VirtualProtect(memory,8192,PAGE_READWRITE,&ignored);memset(memory,0x90,8192);VirtualProtect(memory,8192,PAGE_EXECUTE_READ,&ignored);protects=flushes=writes=0;protectFault=flushFault=writeFault=-1;possible=false;
  if(mode<6)protectFault=(int)mode;else if(mode<9)flushFault=(int)mode-6;else if(mode<12)writeFault=(int)mode-9;else{edits[1].before[0]^=1;}
  C(!code_patch_batch::Apply(edits,3,&threads,&possible));C(threads.count==0);if(mode==12)edits[1].before[0]^=1;
  for(unsigned i=0;i<3;++i)C(memcmp(edits[i].address,edits[i].before,4)==0);
  C(VirtualQuery(memory,&mbi,sizeof(mbi))==sizeof(mbi)&&mbi.Protect==PAGE_EXECUTE_READ);C(VirtualQuery(memory+4096,&mbi,sizeof(mbi))==sizeof(mbi)&&mbi.Protect==PAGE_EXECUTE_READ);
  if(mode==0||mode==1||mode==2||mode==12)C(!possible);else C(possible);
 }
 protects=flushes=writes=0;protectFault=flushFault=writeFault=-1;
 C(!code_patch_batch::Apply(NULL,3,&threads,&possible));C(!code_patch_batch::Apply(edits,0,&threads,&possible));C(!code_patch_batch::Apply(edits,17,&threads,&possible));C(!code_patch_batch::Apply(edits,3,&threads,NULL));
 code_patch_batch::Edit bad[3];memcpy(bad,edits,sizeof(bad));bad[1].address=bad[0].address+1;C(!code_patch_batch::Apply(bad,3,&threads,&possible));memcpy(bad,edits,sizeof(bad));bad[0].size=9;C(!code_patch_batch::Apply(bad,3,&threads,&possible));
 memcpy(bad,edits,sizeof(bad));bad[0].address=memory+4094;C(!code_patch_batch::Apply(bad,3,&threads,&possible));
 VirtualFree(memory,0,MEM_RELEASE);printf("PASS %u code patch batch checks\n",checks);return 0;
}
