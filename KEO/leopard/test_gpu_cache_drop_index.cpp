// Standalone actual-production cache fixture. The header's slot array alone
// is wrapped to count slot touches; production algorithms are included intact.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <map>
#include <algorithm>
static unsigned long touches=0,checks=0,linkTouches=0;
template<class T,int N> struct CountedArray {
    T values[N];
    T& operator[](int i){++touches;return values[i];}
    const T& operator[](int i)const{++touches;return values[i];}
    operator void*(){return values;}
};
template<class T,int N> struct CountedLinks {
    T values[N];
    T& operator[](int i){++linkTouches;return values[i];}
    const T& operator[](int i)const{++linkTouches;return values[i];}
    operator void*(){return values;}
};
#define private public
// CACHE_HEADER
#undef private
// CACHE_IMPLEMENTATION
static void Check(bool yes,const char*what){++checks;if(!yes){printf("FAIL: %s (touches=%lu)\n",what,touches);exit(1);}}
struct Node {MsvcString key; char padding[8]; unsigned long long def;};
static const int N=10000;
static Node nodes[N];static MsvcString names[N];
static const void* Map(unsigned n){return reinterpret_cast<const void*>(0x500000000ULL+size_t(n)*0x48);}
static void Prepare(){for(int i=0;i<N;++i){char text[16];sprintf(text,"c%d",i);size_t n=strlen(text);memcpy(names[i].bx.buf,text,n+1);names[i].size=n;names[i].res=15;nodes[i].key=names[i];nodes[i].def=i+1;}}
// Prior production algorithm is the independent result oracle. It deliberately
// has no map index and uses the original 16-probe/home-replacement policy.
struct Reference {
    struct E {const void*map;const MsvcString*name;const void*def;};E e[GpuParamCache::SLOTS];
    Reference(){Clear();}void Clear(){memset(e,0,sizeof(e));}
    void Store(const void*map,const MsvcString*name,const void*def){size_t h=GpuParamCache::Home(map,name);E*p=NULL;
        for(int i=0;i<GpuParamCache::PROBES&&!p;++i){E&v=e[(h+i)&(GpuParamCache::SLOTS-1)];if(v.map==map&&v.name==name)p=&v;}
        for(int i=0;i<GpuParamCache::PROBES&&!p;++i){E&v=e[(h+i)&(GpuParamCache::SLOTS-1)];if(!v.map)p=&v;}
        if(!p)p=&e[h];p->map=map;p->name=name;p->def=def;}
    const void*Find(const void*map,const MsvcString*name){size_t h=GpuParamCache::Home(map,name);for(int i=0;i<GpuParamCache::PROBES;++i){E&v=e[(h+i)&(GpuParamCache::SLOTS-1)];if(v.map==map&&v.name==name)return v.def;}return NULL;}
    int DropMap(const void*map){int n=0;for(int i=0;i<GpuParamCache::SLOTS;++i)if(e[i].map==map){memset(&e[i],0,sizeof(e[i]));++n;}return n;}
};
static GpuParamCache cache;static Reference ref;
static unsigned Random(){static unsigned x=0x92863f71;x^=x<<13;x^=x>>17;x^=x<<5;return x;}
static void MatchAll(){for(int i=0;i<GpuParamCache::SLOTS;++i){Check(cache.m_slots[i].map==ref.e[i].map,"exact legacy slot map");Check(cache.m_slots[i].name==ref.e[i].name,"exact legacy slot name");Check(cache.m_slots[i].def==ref.e[i].def,"exact legacy slot definition");}
#ifdef INDEX_PRESENT
    bool seen[GpuParamCache::SLOTS]={};int occupied=0;
    for(int b=0;b<GpuParamCache::MAP_BUCKETS;++b){unsigned short next=cache.m_mapHeads[b],previous=0;int chain=0;
        while(next){Check(++chain<=GpuParamCache::SLOTS,"index chain bounded by fixed slot count");int i=next-1;Check(i<GpuParamCache::SLOTS&&!seen[i],"slot indexed exactly once without cycles");seen[i]=true;++occupied;
            Check(cache.m_slots[i].map!=NULL&&GpuParamCache::MapBucket(cache.m_slots[i].map)==size_t(b),"index bucket matches live map");
#ifdef PREV_PRESENT
            Check(cache.m_mapPrev[i]==previous,"reverse link matches predecessor");
#endif
            previous=next;next=cache.m_mapNext[i];}}
    for(int i=0;i<GpuParamCache::SLOTS;++i){Check(seen[i]==(cache.m_slots[i].map!=NULL),"all and only occupied slots indexed");if(!seen[i]){Check(cache.m_mapNext[i]==0,"empty slots hold no stale chain links");
#ifdef PREV_PRESENT
        Check(cache.m_mapPrev[i]==0,"empty slots hold no stale reverse links");
#endif
    }}
    Check(occupied==cache.m_count,"occupancy count matches full actual slot domain");
#endif
}
static void Drop(const void*map){int expected=ref.DropMap(map);Check(cache.DropMap(map)==expected,"exact legacy DropMap result");}
static void Store(unsigned map,int name){linkTouches=0;cache.Store(Map(map),&names[name],&nodes[name].def);Check(linkTouches<=8,"Store index upkeep remains fixed work under collisions");ref.Store(Map(map),&names[name],&nodes[name].def);}
static void Complexity(){
    cache.Clear();ref.Clear();touches=0;Check(cache.DropMap(Map(1))==0,"empty unrelated map drops nothing");Check(touches==0,"empty cache deletion touches no slots");
    Store(1,1);touches=0;Drop(Map(1));Check(touches<=2,"one cached key deletion visits only its chain");
    // Delete an unobserved map from a session-populated cache. Bucket clashes
    // are allowed, but each invalidation cannot scan the entire4096-slot table.
    for(int i=0;i<2000;++i)Store(i%300,i);
    unsigned long maxTouches=0;for(unsigned m=5000;m<6000;++m){touches=0;Drop(Map(m));if(touches>maxTouches)maxTouches=touches;}
    Check(maxTouches<100,"unobserved-map teardown touches only a bounded collision chain");printf("unobserved-map max slot touches=%lu\n",maxTouches);
    MatchAll();
}
static void Stress(){
    cache.Clear();ref.Clear();
    // Saturation forces the legacy home-slot overwrite branch repeatedly.
    for(int i=0;i<N;++i)Store(unsigned(i%3000),i);MatchAll();
    for(int step=0;step<50000;++step){unsigned op=Random()%100;
        if(op<70)Store(Random()%3000,Random()%N);
        else if(op<92)Drop(Map(Random()%3000));
        else if(op<99){unsigned m=Random()%3000;int n=Random()%N;Check(cache.Find(Map(m),&names[n])==ref.Find(Map(m),&names[n]),"legacy lookup agrees after mixed operations");}
        else {cache.Clear();ref.Clear();}
        if(step%1000==0)MatchAll();
    }MatchAll();
    for(unsigned i=0;i<3000;++i)Drop(Map(i));MatchAll();
    touches=0;Drop(Map(1));Check(touches==0,"all maps retired leave zero scan work");
    // Repeated overwrites and deletions at the same raw map/name addresses.
    for(int round=0;round<30;++round){for(int i=0;i<2000;++i)Store(unsigned(i%7),i);for(unsigned m=0;m<7;++m)Drop(Map(m));MatchAll();}
    cache.Clear();ref.Clear();
}
static void BucketClashes(){
#ifdef INDEX_PRESENT
    cache.Clear();ref.Clear();unsigned maps[128],n=0;size_t target=GpuParamCache::MapBucket(Map(1));
    for(unsigned m=1;n<128;++m)if(GpuParamCache::MapBucket(Map(m))==target)maps[n++]=m;
    for(unsigned i=0;i<n;++i)Store(maps[i],i);MatchAll();
    // Same map/name rewrite must not link the same slot twice, and different
    // names sharing one map must all survive until that exact map is dropped.
    for(int repeat=0;repeat<100;++repeat)Store(maps[64],64);
    for(int name=1000;name<1100;++name)Store(maps[64],name);MatchAll();
    Drop(Map(maps[64]));MatchAll();Drop(Map(maps[127]));MatchAll();Drop(Map(maps[0]));MatchAll();
    for(unsigned i=1;i<127;++i)Drop(Map(maps[i]));MatchAll();
    Check(cache.m_count==0,"all colliding maps retired");
    // Preserve even the historical count on the reserved NULL-map query.
    Check(cache.DropMap(NULL)==ref.DropMap(NULL),"reserved NULL DropMap return preserved");
#endif
}
int main(){Prepare();Check(reinterpret_cast<const char*>(&nodes[0].def)-reinterpret_cast<const char*>(&nodes[0].key)==GPC_KEY_TO_DEF,"native key-to-definition offset");Complexity();Stress();BucketClashes();printf("PASS %lu controls\n",checks);return 0;}
