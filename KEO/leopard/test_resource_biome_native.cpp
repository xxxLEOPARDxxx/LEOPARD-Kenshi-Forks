// Native SDK fixtures only: no game process and no engine method is called.
// Include the production implementation to exercise its actual guarded reader.
#include "../src/resources/resource_biome_manifest.cpp"
#include <cstdio>
#include <new>
#include <vector>
intptr_t KlibRealAddressOf(void*){return 0;}
const BiomeMap* ZoneManager::getBiomeMap() const{return NULL;}
namespace {
int checks=0,failures=0;
resource_biome::Status threadStatus=resource_biome::Ok;
DWORD WINAPI OtherThread(void*){
    resource_biome::OwnedFiles out;
    threadStatus=resource_biome::CopyTerrainFiles(NULL,0,out);return 0;
}
#define CHECK(x) do{++checks;if(!(x)){++failures;std::printf("FAIL line %d: %s\n",__LINE__,#x);}}while(0)
struct Fixture {
    __declspec(align(16)) unsigned char storage[sizeof(GameData)];
    typedef decltype(((GameData*)0)->filesdata) Files;
    Files* files;std::string* sid;
    Fixture(){
        std::memset(storage,0,sizeof(storage));
        *(itemType*)(storage+offsetof(GameData,type))=BIOMES;
        sid=new(storage+offsetof(GameData,stringID))std::string("real-biome-SID");
        files=new(storage+offsetof(GameData,filesdata))Files();
    }
    ~Fixture(){files->~Files();sid->~basic_string();}
};
struct ArrayFixture {
    __declspec(align(16)) unsigned char storage[sizeof(ResourceLoader::TextureArrayLoadData)];
    typedef decltype(((ResourceLoader::TextureArrayLoadData*)0)->filesNames) Paths;
    std::string*name,*group;Paths*paths;
    ArrayFixture(){
        std::memset(storage,0,sizeof(storage));
        name=new(storage+8)std::string("Terrain0_real-SID");
        group=new(storage+0x30)std::string("Landscape");
        paths=new(storage+0xA0)Paths(6,"./data/land/base.dds");
    }
    ~ArrayFixture(){paths->~Paths();group->~basic_string();name->~basic_string();}
};
}
int main(){
    using namespace resource_biome;
    Keys keyValues;keys=&keyValues;
    Fixture f;RawFiles raw;
    CHECK(ReadFiles(NULL,0,&raw)==BadBiome);
    CHECK(ReadFiles((void*)1,0,&raw)==ReadFault);
    CHECK(ReadFiles(f.storage,0,&raw)==BadPath);
    (*f.files)["texture base"]="./data/land/base.dds";
    (*f.files)["texture slope"]="./data/land/slope.dds";
    CHECK(ReadFiles(f.storage,0,&raw)==Ok);
    CHECK(std::string(raw.sid)=="real-biome-SID");
    CHECK(std::string(raw.path[0])=="./data/land/base.dds");
    CHECK(std::string(raw.path[1])=="./data/land/slope.dds");
    CHECK(std::string(raw.path[2])=="./data/land/base.dds");
    CHECK(ReadFiles(f.storage,1,&raw)==BadPath);
    (*f.files)["texture base normal"]="./data/land/base_n.dds";
    CHECK(ReadFiles(f.storage,1,&raw)==Ok);
    for(unsigned i=0;i<6;++i)CHECK(std::string(raw.path[i])=="./data/land/base_n.dds");
    (*f.files)["texture dirt"]=std::string("bad\0path",8);
    CHECK(ReadFiles(f.storage,0,&raw)==BadPath);
    (*f.files)["texture dirt"]=std::string(1024,'a');
    CHECK(ReadFiles(f.storage,0,&raw)==BadPath);
    f.files->erase("texture dirt");
    *f.sid=std::string(256,'a');CHECK(ReadFiles(f.storage,0,&raw)==BadBiome);
    *f.sid="";CHECK(ReadFiles(f.storage,0,&raw)==BadBiome);
    *f.sid="restored";
    *(itemType*)(f.storage+offsetof(GameData,type))=BUILDING;
    CHECK(ReadFiles(f.storage,0,&raw)==BadBiome);
    const unsigned char good[]={0x48,0x8b,0x81,0x20,0x82,0x16,0x00,0xc3};
    CHECK(GuardGetter(good)==Ok);CHECK(GuardGetter(good+1)==BadGetter);
    CHECK(GuardGetter((void*)1)==BadGetter);CHECK(GuardGetter(NULL)==BadGetter);
    CHECK(Initialize(GetCurrentThreadId())==BadGetter);
    // The public wrapper owns its strings, enforces the main thread and
    // refuses unknown maps. Fixtures never call engine loading/activation.
    ready=true;mainId=GetCurrentThreadId();
    ArrayFixture af;OwnedArrayFiles array;
    CHECK(CopyArrayFiles(af.storage,array)==Ok&&array.name=="Terrain0_real-SID"&&array.paths[5]=="./data/land/base.dds");
    *af.name="changed";(*af.paths)[0]="changed.dds";
    CHECK(array.name=="Terrain0_real-SID"&&array.paths[0]=="./data/land/base.dds");
    CHECK(CopyArrayFiles(af.storage,array)==BadType&&array.name.empty());
    *af.name="Terrain1_restored";*af.group="Other";CHECK(CopyArrayFiles(af.storage,array)==BadType);
    *af.group="Landscape";af.paths->resize(5);CHECK(CopyArrayFiles(af.storage,array)==BadMetadata);
    af.paths->resize(6,"base.dds");(*af.paths)[1]=std::string("bad\0path",8);CHECK(CopyArrayFiles(af.storage,array)==BadPath);
    (*af.paths)[1]="base.dds";CHECK(CopyArrayFiles(af.storage,array)==Ok);
    *af.name="Terrain1_"+std::string(255,'s');CHECK(CopyArrayFiles(af.storage,array)==Ok&&array.name.size()==264);
    CHECK(CopyArrayFiles((void*)1,array)==ReadFault&&array.name.empty());
    *(itemType*)(f.storage+offsetof(GameData,type))=BIOMES;
    OwnedFiles owned;CHECK(CopyTerrainFiles(f.storage,0,owned)==Ok);
    *f.sid="changed";(*f.files)["texture base"]="changed.dds";
    CHECK(owned.sid=="restored"&&owned.paths[0]=="./data/land/base.dds");
    CHECK(CopyTerrainFiles(f.storage,2,owned)==BadType&&owned.sid.empty());
    HANDLE thread=CreateThread(NULL,0,OtherThread,NULL,0,NULL);
    CHECK(thread!=NULL);
    if(thread){WaitForSingleObject(thread,5000);CloseHandle(thread);CHECK(threadStatus==WrongThread);}
    std::vector<unsigned char> manager(0x168228,0);
    __declspec(align(16)) unsigned char bm[0x60]={0},image[0xd0]={0};
    Slots table[1024];std::memset(table,0,sizeof(table));
    float* world=(float*)(image+0xc0);world[0]=world[1]=-147456;world[2]=world[3]=294912;
    *(void**)bm=image;
    MapHeader* h=(MapHeader*)(bm+0x50);h->fine=h->grid=32;h->table=(unsigned char*)table;
    *(void**)(&manager[0]+0x168220)=bm;
    table[0].tree=(void*)1;table[0].data[0]=f.storage;table[0].data[1]=f.storage;
    BiomePointers pointers;Bounds bounds(-147456,-147456,-142848,-142848);
    CHECK(CandidateBiomes(&manager[0],bounds,pointers)==Ok&&pointers.count==1&&pointers.data[0]==f.storage);
    h->grid=64;CHECK(CandidateBiomes(&manager[0],bounds,pointers)==BadMetadata&&pointers.count==0);h->grid=32;
    world[2]=1;CHECK(CandidateBiomes(&manager[0],bounds,pointers)==BadMetadata);world[2]=294912;
    table[0].tree=NULL;CHECK(CandidateBiomes(&manager[0],bounds,pointers)==BadMetadata);table[0].tree=(void*)1;
    CHECK(CandidateBiomes((void*)1,bounds,pointers)==ReadFault&&pointers.count==0);
    std::printf("resource biome native: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
