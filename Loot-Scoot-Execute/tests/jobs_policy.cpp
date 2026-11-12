#include "../LootScootExecuteJobsPolicy.h"
#include <stdio.h>
#include <stdlib.h>
using namespace lse_jobs;
static int checks=0;
#define CHECK(x) do {++checks;if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static Identity Id(unsigned n) {Identity a={{1,2,3,n,4}};return a;}
int main(){
 Ledger l;Identity victim=Id(1),a=Id(2),b=Id(3);
 CHECK(Custom(294)&&Custom(295)&&!Custom(293)&&!Custom(296));CHECK(l.Count()==0);
 l.Hit(victim,a,false,false,false,0);CHECK(!l.Owns(victim,a));
 l.Hit(victim,a,false,true,false,1);CHECK(l.Owns(victim,a));CHECK(!l.Owns(victim,b));
 l.Hit(victim,b,true,true,false,2);CHECK(l.Owns(victim,a));CHECK(!l.Owns(victim,b));
 Identity recycled=victim;recycled.word[4]++;CHECK(!l.Owns(recycled,a));
 recycled=victim;recycled.word[2]++;CHECK(!l.Owns(recycled,a));
 l.Medical(victim,false,false,3);CHECK(!l.Owns(victim,a));
 l.Hit(victim,b,false,true,false,4);CHECK(l.Owns(victim,b));
 l.Medical(victim,true,true,5);CHECK(!l.Owns(victim,b));CHECK(l.Count()==0);
 l.Record(victim,a,true,100);CHECK(!l.Owns(victim,a));
 l.Medical(victim,false,false,101);CHECK(!l.Owns(victim,a));
 l.Medical(victim,true,false,102);CHECK(l.Owns(victim,a));
 l.Clear();CHECK(!l.Owns(victim,a));
 l.Record(victim,a,true,100);l.Medical(victim,true,false,2100);CHECK(!l.Owns(victim,a));
 l.Record(victim,a,true,0xfffffff0u);l.Medical(victim,true,false,20);CHECK(l.Owns(victim,a));
 CHECK(TargetAllowed(OWN_DEFEATS,true,false,true));
 CHECK(!TargetAllowed(OWN_DEFEATS,true,true,false));
 CHECK(TargetAllowed(ALL_ENEMIES,true,true,false));
 CHECK(!TargetAllowed(ALL_ENEMIES,true,false,true));
 CHECK(!TargetAllowed(ALL_ENEMIES,false,true,true));
 for(unsigned n=0;n<10000;++n)l.Record(Id(n),a,false,n);
 CHECK(!l.Owns(Id(9001),b));CHECK(!l.Owns(Id(9001+10000),a));
 CHECK(l.Count()<=LEDGER_CAP);l.Clear();CHECK(l.Count()==0);
 printf("%d jobs policy checks passed\n",checks);return 0;
}
