#include "inventory/carried_weight_policy.h"
#include <float.h>
#include <math.h>
namespace keo_inventory {
namespace {
bool Finite(float v){return v>=-FLT_MAX&&v<=FLT_MAX;}
bool Nonnegative(float v){return v>=0.0f&&v<=FLT_MAX;}
bool Positive(float v){return v>0.0f&&v<=FLT_MAX;}
float OrOne(float v){return Positive(v)?v:1.0f;}
struct Query {uintptr_t seen[64],path[8];int seenCount,items;bool good;};
bool Seen(Query&q,uintptr_t inv){for(int i=0;i<q.seenCount;++i)if(q.seen[i]==inv)return true;return false;}
float Visit(const WeightOps&o,uintptr_t inv,float base,Query&q,int depth,bool carriedLoad);
float Child(const WeightOps&o,uintptr_t inv,Query&q,int depth,bool carriedLoad){
 if(!inv)return 0;
 for(int i=0;i<depth&&i<8;++i)if(q.path[i]==inv){q.good=false;return 0;}
 if(Seen(q,inv))return 0;
 if(depth>=8||q.seenCount==64){q.good=false;return 0;}
 return Visit(o,inv,o.nativeWeight(o.ctx,inv),q,depth,carriedLoad);
}
float Visit(const WeightOps&o,uintptr_t inv,float base,Query&q,int depth,bool carriedLoad){
 if(!Nonnegative(base)||depth>=8||q.seenCount==64){q.good=false;return 0;}
 q.seen[q.seenCount++]=inv;q.path[depth]=inv;
 float total=base,nativeBody=0;int n=o.count(o.ctx,inv);
 if(n<0||n>4096-q.items){q.good=false;return 0;}q.items+=n;
 for(int i=0;i<n&&q.good;++i){uintptr_t bag=o.child(o.ctx,inv,i);if(bag)total+=Child(o,bag,q,depth+1,false);}
 uintptr_t owner=o.owner(o.ctx,inv);uintptr_t held=owner?o.carried(o.ctx,owner):0;
 if(held&&q.good){
  total+=Child(o,o.inventory(o.ctx,held),q,depth+1,true);
  WeightBody b={5,17,5,1,100,100,3,0};
  if(!o.nativeBody(o.ctx,owner,inv,&nativeBody)||!Nonnegative(nativeBody)){q.good=false;return 0;}
  float factor=o.body(o.ctx,held,&b)?CarriedBodyFactor(b):1.0f;
  // A carried inventory contributes its complete load, including its own held body.
  // The root remains a signed correction: native CharacterCarry adds that body.
  total+=nativeBody*(factor-(carriedLoad?0.0f:1.0f));
 }
 double finalCarry=(double)total+(carriedLoad?0.0:nativeBody);
 if(!Finite(total)||finalCarry<0||finalCarry>FLT_MAX)q.good=false;
 return total;
}
}
float CarriedBodyFactor(const WeightBody&b){
 double volume=1.0;
 if(Positive(b.x)&&Positive(b.y)&&Positive(b.z))volume=(double)b.x*b.y*b.z/425.0;
 double scale=OrOne(b.scale),height=OrOne(b.height)/100.0,frame=OrOne(b.frame)/100.0;
 // Missing/nonpositive sliders are native defaults, not1percent.
 if(!Positive(b.height))height=1;if(!Positive(b.frame))frame=1;
 volume*=scale*scale*scale*height*height*height*frame;
 double factor=volume>0?sqrt(volume):1.0;
 if(b.robot)factor*=1.2;
 else if(b.hunger>=-FLT_MAX&&b.hunger<=FLT_MAX){float h=b.hunger;if(h<0)h=0;if(h>3)h=3;factor*=.7+.3*(double)h/3.0;}
 return factor>0&&factor<=FLT_MAX?(float)factor:1.0f;
}
float CarriedWeightEvaluate(const WeightOps&o,uintptr_t inv,float base){
 if(!inv||!Nonnegative(base)||!o.nativeWeight||!o.owner||!o.count||!o.child||!o.carried||!o.inventory||!o.body||!o.nativeBody)return base;
 Query q={};q.good=true;float value=Visit(o,inv,base,q,0,false);return q.good?value:base;
}
}
