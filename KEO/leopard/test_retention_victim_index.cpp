#include <map>
#include <iostream>
#include <cstdlib>
#include <new>
#include "resources/retention_victim_index.h"
static unsigned allocations=0,checks=0;
void*operator new(size_t n){++allocations;void*p=malloc(n?n:1);if(!p)throw std::bad_alloc();return p;}
void operator delete(void*p){free(p);}
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"line "<<__LINE__<<": "<<#x<<"\n";return false;}}while(0)
struct Entry {unsigned __int64 touch;unsigned victimSlot;Entry():touch(0),victimSlot(~0u){}};
struct Keys {bool reverse;Keys(bool r=false):reverse(r){}bool operator()(const int*a,const int*b)const{return reverse?std::less<const int*>()(b,a):std::less<const int*>()(a,b);}};
typedef std::map<const int*,Entry,Keys> Map;
static unsigned randomState=0x19218041;
static unsigned Random(){randomState=randomState*1664525u+1013904223u;return randomState>>8;}
static Map::iterator Oracle(Map&m){Map::iterator o=m.begin();for(Map::iterator i=m.begin();i!=m.end();++i)if(i->second.touch<o->second.touch)o=i;return o;}
template<unsigned Capacity> static bool Differential(bool reverse){
    Map map((Keys(reverse)));resource_retention::VictimIndex<Map,Capacity> index(map);static int keys[Capacity+1];
    for(unsigned n=0;n<Capacity;++n){Entry e;e.touch=n%17;Map::iterator i=map.insert(std::make_pair(&keys[n],e)).first;unsigned a=allocations;CHECK(index.Insert(i));CHECK(allocations==a);CHECK(index.Oldest()==Oracle(map));}
    Entry e;Map::iterator overflow=map.insert(std::make_pair(&keys[Capacity],e)).first;
    CHECK(!index.Insert(overflow));CHECK(overflow->second.victimSlot==~0u);map.erase(overflow);
    CHECK(!index.Insert(map.begin()));CHECK(index.Size()==Capacity);
    unsigned __int64 clock=~0ULL-100;
    // Exact numeric ordering across wrap, increasing and decreasing touches,
    // ties, arbitrary removals, refresh-style reinsertion with OLD touch.
    unsigned actions[4]={};
    for(unsigned n=0;n<30000;++n){
        unsigned id=Random()%Capacity;Map::iterator i=map.find(&keys[id]);CHECK(i!=map.end());
        unsigned action=Random()%4;++actions[action];unsigned a=allocations;
        if(action==0){i->second.touch=++clock;index.Changed(i);CHECK(allocations==a);}
        else if(action==1){i->second.touch=Random()%17;index.Changed(i);CHECK(allocations==a);}
        else if(action==2){unsigned __int64 old=i->second.touch;index.Remove(i);CHECK(allocations==a);map.erase(i);Entry fresh;fresh.touch=old;i=map.insert(std::make_pair(&keys[id],fresh)).first;a=allocations;CHECK(index.Insert(i));CHECK(allocations==a);}
        else{CHECK(index.Oldest()==Oracle(map));}
        CHECK(index.Size()==map.size());CHECK(index.Oldest()==Oracle(map));
        if(n%251==0){bool slots[Capacity]={};for(Map::iterator j=map.begin();j!=map.end();++j){CHECK(j->second.victimSlot<Capacity);CHECK(!slots[j->second.victimSlot]);slots[j->second.victimSlot]=true;}}
    }
    for(unsigned action=0;action<4;++action)CHECK(actions[action]>5000);
    std::cout<<"actions="<<actions[0]<<","<<actions[1]<<","<<actions[2]<<","<<actions[3]<<"\n";
    while(!map.empty()){Map::iterator i=Oracle(map);CHECK(index.Oldest()==i);unsigned a=allocations;index.Remove(i);CHECK(allocations==a);map.erase(i);}
    CHECK(index.Oldest()==map.end()&&index.Size()==0);
    for(unsigned n=0;n<Capacity;++n){Entry e;e.touch=0;CHECK(index.Insert(map.insert(std::make_pair(&keys[n],e)).first));}
    unsigned a=allocations;index.Reset();CHECK(allocations==a);CHECK(index.Size()==0&&index.Oldest()==map.end());
    for(Map::iterator i=map.begin();i!=map.end();++i){CHECK(i->second.victimSlot==~0u);CHECK(index.Insert(i));}
    CHECK(index.Oldest()==Oracle(map));index.Reset();map.clear();
    std::cout<<"capacity="<<Capacity<<" reverse="<<reverse<<" bytes="<<sizeof(index)<<" entry="<<sizeof(Entry)<<"\n";return true;
}
int main(){if(!Differential<2048>(false)||!Differential<1024>(false)||!Differential<2048>(true)||!Differential<1024>(true))return 1;std::cout<<"victim index checks="<<checks<<" allocations outside index="<<allocations<<"\n";return 0;}
