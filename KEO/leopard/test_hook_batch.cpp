#include <cstdio>
#include <cstring>
#include "plugin/independent_hook_batch.h"
static unsigned checks=0,queues=0,applies=0,completions=0,immediates=0;
static int queueFail=-1,applyFail=-1,queueThrow=-1,queueNull=-1;static bool applyThrow=false,reentry=false;static void* dummy=NULL;static size_t wantedId=0x11110000;
static void**slots[16];static int seen[16];static HookBatch*current=NULL;
#define C(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL %u %s\n",__LINE__,#x);return 1;}}while(0)
static void Complete(int,const char*);
static int Queue(size_t id,void*t,void*,void**o){if(id!=wantedId)throw 11;int n=int(queues++);slots[n]=o;*o=n==queueNull?NULL:(void*)(size_t(0x1000)+size_t(t));if(reentry&&current){if(current->Collecting()||current->Add(15,(void*)1,(void*)2,&dummy,Complete))throw 91;}if(n==queueThrow)throw 92;return n==queueFail?1:0;}
static int Apply(size_t id){if(id!=wantedId)throw 12;++applies;if(applyThrow)throw 93;for(unsigned i=0;i<queues;++i)if(!*slots[i])throw 13;return applyFail;}
static void Complete(int row,const char*why){++completions;seen[row]=why?1:2;if(current&&current->Collecting())throw 14;}
static void Reset(){queues=applies=completions=immediates=0;queueFail=queueThrow=queueNull=-1;applyFail=0;applyThrow=reentry=false;memset(seen,0,sizeof(seen));current=NULL;}
int main(){
 for(int bad=-1;bad<4;++bad)for(int fail=0;fail<2;++fail){
  Reset();queueFail=bad;applyFail=fail;HookBatch b;current=&b;C(b.Begin(wantedId,Queue,Apply));void*original[4]={0};
  for(int i=0;i<4;++i)C(b.Add(i,(void*)(size_t(i)+1),(void*)55,&original[i],Complete));
  C(queues==0&&applies==0&&completions==0);b.Finish();C(queues==4&&applies==1&&completions==4);
  for(int i=0;i<4;++i){C(original[i]==(void*)(size_t(0x1001)+i));C(seen[i]==((i==bad||fail)?1:2));}
  C(!b.Collecting());b.Finish();C(queues==4&&applies==1&&completions==4);
  C(!b.Begin(wantedId,Queue,Apply));C(!b.Add(0,(void*)1,(void*)2,&original[0],Complete));
 }
 Reset();HookBatch unsupported;C(!unsupported.Begin(0,Queue,Apply));C(!unsupported.Collecting());unsupported.Finish();C(queues==0&&applies==0);
 HookBatch empty;C(empty.Begin(wantedId,Queue,Apply));empty.Finish();C(applies==0);
 Reset();HookBatch bounded;C(bounded.Begin(wantedId,Queue,Apply));void*o[HookBatch::CAPACITY]={0};for(int i=0;i<HookBatch::CAPACITY;++i)C(bounded.Add(i,(void*)(size_t(i)+1),(void*)2,&o[i],Complete));C(!bounded.Add(0,(void*)1,(void*)2,&o[0],Complete));bounded.Finish();C(queues==HookBatch::CAPACITY&&applies==1);

 for(int fault=0;fault<4;++fault){Reset();HookBatch b;current=&b;reentry=true;if(fault==0)queueThrow=1;if(fault==1)queueNull=2;if(fault==2)applyThrow=true;C(b.Begin(wantedId,Queue,Apply));void* o[4]={0};for(int i=0;i<4;++i)C(b.Add(i,(void*)(size_t(i)+1),(void*)2,&o[i],Complete));C(!b.Add(0,(void*)1,(void*)2,&o[0],Complete));b.Finish();C(queues==4&&applies==(fault==1?0:1)&&completions==4);for(int i=0;i<4;++i){C(o[i]==(i==queueNull?NULL:(void*)(size_t(0x1001)+i)));C(seen[i]==(fault==1||fault==2||i==queueThrow?1:2));}b.Finish();C(completions==4);}
 Reset();HookBatch invalid;C(!invalid.Begin(wantedId,NULL,Apply));C(!invalid.Begin(wantedId,Queue,Apply));HookBatch invalidApply;C(!invalidApply.Begin(wantedId,Queue,NULL));HookBatch args;C(args.Begin(wantedId,Queue,Apply));void* invalidO=NULL;C(!args.Add(0,NULL,(void*)1,&invalidO,Complete));C(!args.Add(0,(void*)1,NULL,&invalidO,Complete));C(!args.Add(0,(void*)1,(void*)2,NULL,Complete));C(!args.Add(0,(void*)1,(void*)2,&invalidO,NULL));args.Finish();C(queues==0&&applies==0);
 std::printf("PASS %u actual hook batch controls\n",checks);return 0;
}
