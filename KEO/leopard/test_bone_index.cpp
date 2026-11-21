#define NOMINMAX
#include <Windows.h>
#include <ogre/OgreRoot.h>
#include <ogre/OgreMeshManager.h>
#include <ogre/OgreMeshSerializer.h>
#include <ogre/OgreDefaultHardwareBufferManager.h>
#include <ogre/OgreSubMesh.h>
#include <ogre/OgreDataStream.h>
#include <xmmintrin.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <fstream>
#include <stdexcept>
#include "fixes/near_page.h"
#include "fixes/code_patch_batch.h"
#include "fixes/world/bone_index.h"
namespace fixes{struct Config{bool meshWeightsEnabled;};static Config g_fixesCfg={true};}
static DWORD ownerThread=GetCurrentThreadId();static bool IsMainThread(){return GetCurrentThreadId()==ownerThread;}
static bool imageIdEnabled=true;
static bool ReadModuleImageId(HMODULE module,DWORD*stamp,DWORD*size){if(!imageIdEnabled)return false;IMAGE_DOS_HEADER*d=(IMAGE_DOS_HEADER*)module;IMAGE_NT_HEADERS64*n=(IMAGE_NT_HEADERS64*)((char*)module+d->e_lfanew);*stamp=n->FileHeader.TimeDateStamp;*size=n->OptionalHeader.SizeOfImage;return true;}
#define KEO_BONE_INDEX_TEST
#include "../src/fixes/world/bone_index_patch.cpp"
typedef Ogre::Mesh::VertexBoneAssignmentList List;typedef Ogre::Mesh::IndexMap Map;
typedef void(*Native)(Ogre::Mesh*,const List&,Map&,Map&);
static Native native=NULL;static bone_index::Grow grow=NULL;
static unsigned checks=0,growthCalls=0,originalCalls=0,mode=0,throwOn=0;static List*source=NULL;static Map*extra=NULL;
#define C(x) do{++checks;if(!(x)){printf("FAIL line %u: %s\n",__LINE__,#x);return 1;}}while(0)
static void Grow(void*self,size_t delta){++growthCalls;Map*array=(Map*)self;
 if(throwOn&&growthCalls==throwOn){size_t capacity=(std::max)(array->size()+delta,array->capacity()+(array->capacity()>>1)+1);memcpy((char*)self+16,&capacity,8);throw std::bad_alloc();}
 if(growthCalls==1){if(mode==1)array->resize(0);if(mode==2)array->resize(99,0x77aa);if(mode==3)array->swap(*extra);if(mode==4)source->clear();}
 grow(self,delta);
}
static void Original(void*self,const void*list,void*a,void*b){++originalCalls;native((Ogre::Mesh*)self,*(const List*)list,*(Map*)a,*(Map*)b);}
#include "bone_index_native/fixture_hooks.h"
static void Add(List&list,unsigned vertex,unsigned bone,unsigned bits=0x3f000000){Ogre::VertexBoneAssignment value;memset(&value,0xa5,sizeof(value));value.vertexIndex=vertex;value.boneIndex=(unsigned short)bone;memcpy(&value.weight,&bits,4);list.insert(List::value_type(vertex,value));}
static bool Same(const Map&a,const Map&b){return a.size()==b.size()&&a.capacity()==b.capacity()&&(!a.size()||!memcmp(a.begin(),b.begin(),a.size()*2));}
static void Prepare(Map&a,unsigned old,unsigned capacity){a.reserve(capacity);a.resize(capacity);for(unsigned i=0;i<capacity;++i)a[i]=(unsigned short)(0x8000+i);a.resize(old);}
static int Oracle(Ogre::Mesh*m,const List&list,unsigned oldA=0,unsigned capA=0,unsigned oldB=0,unsigned capB=0){Map a,b,x,y;Prepare(a,oldA,capA);Prepare(x,oldA,capA);Prepare(b,oldB,capB);Prepare(y,oldB,capB);native(m,list,a,b);growthCalls=0;unsigned csr=_mm_getcsr();C(bone_index::Try(&list,&x,&y,Grow));C(_mm_getcsr()==csr);C(Same(a,x));C(Same(b,y));return 0;}
static int Cases(Ogre::Mesh*m){
 List repeated;for(unsigned i=0;i<32000;++i)Add(repeated,i/3,(i*13)%73);C(Oracle(m,repeated)==0);C(Oracle(m,repeated,199,256,119,256)==0);C(Oracle(m,repeated,3,4,2,4)==0);
 List keys;for(unsigned i=0;i<65536;++i)Add(keys,65535-i,i,i%2?0x7f800001:0xff800000);C(Oracle(m,keys)==0);C(Oracle(m,keys,8,65536,3,65536)==0);
 List sparse;for(unsigned i=0;i<128;++i)Add(sparse,i,i%3?65535:0);C(Oracle(m,sparse,16,65536,17,32)==0);return 0;
}
static int Integration(Ogre::Mesh*m,HMODULE dll){
 C(bone_index_patch::Admit(dll)==(char*)dll+0x1a5e30);imageIdEnabled=false;C(!bone_index_patch::Admit(dll));imageIdEnabled=true;C(!bone_index_patch::Admit(NULL));DWORD saved=ownerThread;ownerThread=0;C(!bone_index_patch::Admit(dll));ownerThread=saved;
 std::vector<unsigned char> image(0x9c9000,0);for(unsigned k=0;k<sizeof(bone_index_patch::proofs)/sizeof(bone_index_patch::proofs[0]);++k){const bone_index_patch::Proof&p=bone_index_patch::proofs[k];memcpy(&image[p.rva],p.bytes,p.size);}C(bone_index_patch::Validate(&image[0],image.size()));C(!bone_index_patch::Validate(&image[0],image.size()-1));
 for(unsigned k=0;k<sizeof(bone_index_patch::proofs)/sizeof(bone_index_patch::proofs[0]);++k){const bone_index_patch::Proof&p=bone_index_patch::proofs[k];for(unsigned j=0;j<p.size;++j){image[p.rva+j]^=1;C(!bone_index_patch::Validate(&image[0],image.size()));image[p.rva+j]^=1;}}
 bone_index_patch::original=Original;bone_index_patch::grow=Grow;List input;for(unsigned i=0;i<128;++i)Add(input,i,i%37);
 for(unsigned gate=0;gate<4;++gate){Map a,b;originalCalls=0;bone_index_patch::admitted=gate!=1;fixes::g_fixesCfg.meshWeightsEnabled=gate!=2;if(gate==3)ownerThread=0;bone_index_patch::Build(m,&input,&a,&b);C(originalCalls==(gate?1u:0u));C(a.size()==37&&b.size()==37);ownerThread=saved;}
 bone_index_patch::admitted=true;fixes::g_fixesCfg.meshWeightsEnabled=true;
 List tiny;Add(tiny,7,2);Map a,b;Prepare(a,13,99);Prepare(b,17,99);Map x(a),y(b);C(!bone_index::Try(&tiny,&a,&b,Grow));C(a.size()==13&&b.size()==17);List big;for(unsigned i=0;i<128;++i)Add(big,i,7);size_t*count=(size_t*)((char*)&big+16);size_t old=*count;*count=1048577;C(!bone_index::Try(&big,&a,&b,Grow));*count=old;C(!bone_index::Try(&big,&a,&a,Grow));C(!bone_index::Try(NULL,&a,&b,Grow));C(!bone_index::Try(&big,&a,&b,NULL));
 Map emptyAlias;C(!bone_index::Try(&big,&emptyAlias,&emptyAlias,Grow));
 unsigned char savedMap[24];memcpy(savedMap,&b,24);memcpy(&b,&a,24);C(!bone_index::Try(&big,&a,&b,Grow));memcpy(&b,savedMap,24);
 size_t cap=a.capacity(),invalid=cap+1;memcpy((char*)&a+8,&invalid,8);C(!bone_index::Try(&big,&a,&b,Grow));invalid=13;memcpy((char*)&a+8,&invalid,8);
 // A late metadata mismatch must reject without touching outputs.
 *count=old+1;C(!bone_index::Try(&big,&a,&b,Grow));*count=old;C(a.size()==13&&b.size()==17);return 0;
}
static int Reentry(Ogre::Mesh*m){
 for(unsigned trial=0;trial<6;++trial){Map a,b,x,y,spareA,spareB;unsigned capacity=trial<4?256:4;Prepare(a,3,capacity);Prepare(x,3,capacity);Prepare(b,2,capacity);Prepare(y,2,capacity);Prepare(spareA,7,256);Prepare(spareB,7,256);List expected,actual;for(unsigned i=0;i<256;++i){Add(expected,i,i%73);Add(actual,i,i%73);}mode=trial<4?trial+1:0;throwOn=trial<4?0:trial-3;growthCalls=0;source=&expected;extra=&spareA;bool threwA=false,threwB=false;
  try{native(m,expected,a,b);}catch(const std::bad_alloc&){threwA=true;}
  unsigned wantCalls=growthCalls;growthCalls=0;source=&actual;extra=&spareB;
  try{C(bone_index::Try(&actual,&x,&y,Grow));}catch(const std::bad_alloc&){threwB=true;}
  C(threwA==threwB);C(growthCalls==wantCalls);C(Same(a,x));C(Same(b,y));C(Same(spareA,spareB));C(actual.size()==expected.size());
 }
 mode=throwOn=0;source=NULL;extra=NULL;return 0;
}
static int Assets(Ogre::Mesh*m){const char*paths[]={"human/human_male.mesh","human/human_female.mesh","stick_person/Stick_person.mesh","stick_person/Hive_Queen.mesh","robot/robot.mesh"};unsigned total=0;
 for(unsigned k=0;k<5;++k){std::string path="D:/SteamLibrary/steamapps/common/Kenshi/data/character/meshes/";path+=paths[k];std::ifstream*file=new std::ifstream(path.c_str(),std::ios::binary);C(file->good());Ogre::DataStreamPtr stream(OGRE_NEW Ogre::FileStreamDataStream(path,file,false));char name[40];sprintf_s(name,"index-offline-%u",k);Ogre::MeshPtr mesh=Ogre::MeshManager::getSingleton().createManual(name,"General",NULL);Ogre::MeshSerializer serializer;serializer.importMesh(stream,mesh.get());
  for(unsigned sub=0;sub<mesh->getNumSubMeshes();++sub){const List&list=mesh->getSubMesh((unsigned short)sub)->getBoneAssignments();if(list.size()<128)continue;C(Oracle(m,list)==0);C(Oracle(m,list,99,256,99,256)==0);total+=(unsigned)list.size();printf("ASSET %s assignments=%u\n",paths[k],(unsigned)list.size());}
  stream.setNull();delete file;
 }C(total==135745);printf("ASSET TOTAL %u assignments\n",total);return 0;
}
static int Benchmark(Ogre::Mesh*m){LARGE_INTEGER freq;QueryPerformanceFrequency(&freq);bone_index_patch::grow=grow;
 const unsigned counts[]={128,32000,32000,65536,128,128},bones[]={73,73,65536,65536,65536,65536};for(unsigned pattern=0;pattern<6;++pattern){List list;unsigned random=17;for(unsigned i=0;i<counts[pattern];++i){random=random*1664525+1013904223;Add(list,i,pattern>=4?(i%2?65535:0):pattern==3?i:random%bones[pattern]);}double us[2]={};
  for(unsigned round=0;round<24;++round)for(unsigned order=0;order<2;++order){unsigned fast=(round+order)%2;Map a,b;if(pattern==5){Prepare(a,65536,65536);Prepare(b,16,16);}LARGE_INTEGER start,end;QueryPerformanceCounter(&start);if(fast)bone_index_patch::Build(m,&list,&a,&b);else native(m,list,a,b);QueryPerformanceCounter(&end);us[fast]+=double(end.QuadPart-start.QuadPart)*1e6/freq.QuadPart/24;C(a.size()&&b.size());}
  printf("BENCH assignments=%u domain=%u retained=%u native_us=%.2f optimized_us=%.2f ratio=%.3f\n",counts[pattern],bones[pattern],pattern==5?1:0,us[0],us[1],us[1]/us[0]);
 }return 0;
}
int main(int argc,char**argv){setvbuf(stdout,NULL,_IONBF,0);Ogre::Root root("","","");Ogre::DefaultHardwareBufferManager hardware;HMODULE dll=GetModuleHandleA("OgreMain_x64.dll");C(dll!=NULL);native=(Native)((char*)dll+0x1a5e30);grow=(bone_index::Grow)((char*)dll+0x1b0570);Ogre::MeshPtr m=Ogre::MeshManager::getSingleton().createManual("bone-index-oracle","General",NULL);C(Integration(m.get(),dll)==0);FixtureHooks hooks;C(hooks.Arm(dll)==0);C(!bone_index_patch::Admit(dll));C(Cases(m.get())==0);C(Reentry(m.get())==0);C(hooks.Restore()==0);C(bone_index_patch::Admit(dll)!=NULL);if(argc>1&&!strcmp(argv[1],"assets"))C(Assets(m.get())==0);else if(argc>1)C(Benchmark(m.get())==0);printf("PASS %u native bone index checks\n",checks);return 0;}
