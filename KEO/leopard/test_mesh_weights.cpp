#include <Windows.h>
#include <ogre/OgreRoot.h>
#include <ogre/OgreMesh.h>
#include <ogre/OgreMeshManager.h>
#include <ogre/OgreMeshSerializer.h>
#include <ogre/OgreDefaultHardwareBufferManager.h>
#include <ogre/OgreSubMesh.h>
#include <ogre/OgreDataStream.h>
#include <xmmintrin.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>
#include "fixes/world/mesh_weights_linear.h"
#include "plugin/hook_manifest.h"
namespace fixes{struct Config{bool meshWeightsEnabled;};static Config g_fixesCfg={true};}
static DWORD ownerThread=GetCurrentThreadId();
static bool IsMainThread(){return GetCurrentThreadId()==ownerThread;}
static bool imageIdEnabled=true;
static bool ReadModuleImageId(HMODULE module,DWORD*stamp,DWORD*size){if(!imageIdEnabled)return false;IMAGE_DOS_HEADER*dos=(IMAGE_DOS_HEADER*)module;IMAGE_NT_HEADERS64*nt=(IMAGE_NT_HEADERS64*)((char*)module+dos->e_lfanew);*stamp=nt->FileHeader.TimeDateStamp;*size=nt->OptionalHeader.SizeOfImage;return true;}
static bool useQueue=false;
static unsigned hookCalls=0,installErrors=0;
static void* nextOriginal=NULL;
static StartupAuxHookCompletion queuedDone=NULL;
static void** queuedOriginal=NULL;
bool QueueStartupAuxHook(StartupAuxHookId id,void*,void*,void** original,StartupAuxHookCompletion done){
 if(!useQueue||id!=STARTUP_AUX_MESH_WEIGHTS)return false;
 queuedDone=done;queuedOriginal=original;return true;
}
namespace KenshiLib {
enum Status{SUCCESS};
static Status AddHook(void*,void*,void** original){++hookCalls;*original=nextOriginal;return SUCCESS;}
}
static void LogError(const std::string&){++installErrors;}
#define KEO_DETAIL(x) ((void)0)
#define KEO_MESH_WEIGHT_TEST
#define KEO_MESH_WEIGHT_INIT_TEST
#include "../src/fixes/world/mesh_weights.cpp"
static unsigned checks=0;
#define C(x) do{++checks;if(!(x)){printf("FAIL line %u: %s\n",__LINE__,#x);return 1;}}while(0)
typedef Ogre::Mesh::VertexBoneAssignmentList List;
static void Add(List&m,unsigned v,unsigned bone,float w){Ogre::VertexBoneAssignment b={v,(unsigned short)bone,w};m.insert(std::make_pair(size_t(v),b));}
static bool Same(const List&a,const List&b){if(a.size()!=b.size())return false;List::const_iterator x=a.begin(),y=b.begin();for(;x!=a.end();++x,++y)if(x->first!=y->first||x->second.vertexIndex!=y->second.vertexIndex||x->second.boneIndex!=y->second.boneIndex||memcmp(&x->second.weight,&y->second.weight,4))return false;return true;}
static unsigned fallbackCalls=0;
static unsigned short Native(Ogre::Mesh*m,size_t n,List&list){++fallbackCalls;return m->_rationaliseBoneAssignments(n,list);}
static int Integration(Ogre::Mesh*m){
 HMODULE module=GetModuleHandleA("OgreMain_x64.dll");C(mesh_weights_patch::Admit(module)==(char*)module+0x1a5510);imageIdEnabled=false;C(!mesh_weights_patch::Admit(module));imageIdEnabled=true;C(!mesh_weights_patch::Admit(NULL));
 std::vector<unsigned char>image(0x9c9000,0);for(unsigned k=0;k<sizeof(mesh_weights_patch::proofs)/sizeof(mesh_weights_patch::proofs[0]);++k){const mesh_weights_patch::Proof&p=mesh_weights_patch::proofs[k];memcpy(&image[p.rva],p.bytes,p.size);}C(mesh_weights_patch::Validate(&image[0],image.size()));C(!mesh_weights_patch::Validate(&image[0],image.size()-1));
 for(unsigned k=0;k<sizeof(mesh_weights_patch::proofs)/sizeof(mesh_weights_patch::proofs[0]);++k){const mesh_weights_patch::Proof&p=mesh_weights_patch::proofs[k];for(unsigned j=0;j<p.size;++j){image[p.rva+j]^=1;C(!mesh_weights_patch::Validate(&image[0],image.size()));image[p.rva+j]^=1;}}
 mesh_weights_patch::original=&Native;
 for(unsigned mode=0;mode<5;++mode){List input;Add(input,0,7,.4f);List expected(input);unsigned short want=m->_rationaliseBoneAssignments(1,expected);fixes::g_fixesCfg.meshWeightsEnabled=mode!=1;mesh_weights_patch::admitted=mode!=2;DWORD saved=ownerThread;if(mode==3)ownerThread=0;if(mode==4){input.begin()->second.weight=2;expected=input;want=m->_rationaliseBoneAssignments(1,expected);}fallbackCalls=0;C(mesh_weights_patch::Rationalise(m,1,input)==want);C(Same(input,expected));C(fallbackCalls==(mode?1u:0u));ownerThread=saved;}
 nextOriginal=(void*)&Native;
 for(unsigned mode=0;mode<4;++mode){
  useQueue=mode!=0;queuedDone=NULL;hookCalls=installErrors=0;
  mesh_weights_patch::tried=mesh_weights_patch::admitted=false;
  mesh_weights_patch::original=NULL;
  mesh_weights_patch::Install();
  if(useQueue){
   C(queuedDone&&hookCalls==0&&!mesh_weights_patch::admitted);
   if(mode!=3)*queuedOriginal=nextOriginal;
   queuedDone(STARTUP_AUX_MESH_WEIGHTS,mode==2?"batch-apply":NULL);
  }
  C(hookCalls==(useQueue?0u:1u)&&installErrors==(mode>1?1u:0u));
  C(mesh_weights_patch::admitted==(mode<2)&&mesh_weights_patch::original==(mode==3?NULL:&Native));
 }
 useQueue=false;mesh_weights_patch::original=&Native;
 fixes::g_fixesCfg.meshWeightsEnabled=true;mesh_weights_patch::admitted=true;return 0;
}
static int Oracle(Ogre::Mesh*m,const List&input,size_t vertices,bool eligible){
 const unsigned saved=_mm_getcsr();
 for(unsigned rounding=0;rounding<4;++rounding)for(unsigned denormal=0;denormal<4;++denormal){List expected(input),actual(input);std::vector<const void*>nodes;for(List::const_iterator it=actual.begin();it!=actual.end();++it)nodes.push_back(&*it);unsigned control=0x1f80|(rounding<<13)|((denormal&1)?0x40u:0)|((denormal&2)?0x8000u:0);_mm_setcsr(control);unsigned short native=m->_rationaliseBoneAssignments(vertices,expected);unsigned nativeFlags=_mm_getcsr()&63;_mm_setcsr(control);unsigned short got=0xcccc;bool accepted=mesh_weights::Try(vertices,actual,&got);unsigned flags=_mm_getcsr()&63;C(accepted==eligible);if(accepted){C(got==native);C(Same(expected,actual));C(flags==nativeFlags);}else{C(Same(input,actual));C(got==0xcccc);C(_mm_getcsr()==control);}unsigned node=0;for(List::const_iterator it=actual.begin();it!=actual.end();++it,++node)C(&*it==nodes[node]);_mm_setcsr(saved);}
 return 0;
}
static int Cases(Ogre::Mesh*m){
 for(unsigned n=1;n<=4;++n){List input;for(unsigned v=0;v<13;++v)for(unsigned k=0;k<n;++k)Add(input,v,k,(k+1)*.17f);C(Oracle(m,input,13,true)==0);}
 {List input;Add(input,0,0,1);Add(input,1,0,0);Add(input,1,1,1);C(Oracle(m,input,2,true)==0);}
 {List input;Add(input,0,0,1);Add(input,2,0,1);C(Oracle(m,input,3,false)==0);}
 {List input;for(unsigned k=0;k<5;++k)Add(input,0,k,.2f);C(Oracle(m,input,1,false)==0);}
 {List input;Add(input,0,0,0);C(Oracle(m,input,1,false)==0);}
 {List input;Add(input,0,0,1);Add(input,5,0,1);C(Oracle(m,input,1,false)==0);}
 {List input;Add(input,0,0,1);input.begin()->second.vertexIndex=7;C(Oracle(m,input,1,false)==0);}
 const unsigned exceptional[]={0x80000000u,1u,0x007fffffu,0x7f800000u,0x7fc12345u,0x7f812345u,0xbf000000u,0x40000000u};
 for(unsigned n=0;n<sizeof(exceptional)/sizeof(exceptional[0]);++n){List input;float w;memcpy(&w,&exceptional[n],4);Add(input,0,0,1);Add(input,1,0,w);C(Oracle(m,input,2,false)==0);}
 const float edges[]={1.1920928955078125e-7f,2.384185791015625e-7f,.33333334f,.00003125f};
 for(unsigned n=0;n<4;++n){List input;Add(input,0,0,1);Add(input,0,1,edges[n]);Add(input,1,0,edges[n]);C(Oracle(m,input,2,true)==0);}
 {List input;Add(input,0,0,1);unsigned short out=77;const unsigned saved=_mm_getcsr();_mm_setcsr(saved&~0x80u);C(!mesh_weights::Try(1,input,&out));C(out==77&&input.begin()->second.weight==1);_mm_setcsr(saved);C(!mesh_weights::Try(0,input,&out));C(!mesh_weights::Try(1048577,input,&out));C(!mesh_weights::Try(1,input,NULL));}
 {List input;float tiny;unsigned bits=0x00800000u;memcpy(&tiny,&bits,4);Add(input,0,7,tiny);Add(input,1,9,tiny);Add(input,1,9,1);Add(input,1,2,1);Add(input,1,1,1);C(Oracle(m,input,2,true)==0);}
 {List input;for(unsigned v=0;v<64;++v)for(unsigned k=0;k<v%4+1;++k)Add(input,63-v,(k%2)?19:3,.17f*(k+1));C(Oracle(m,input,64,true)==0);for(unsigned k=0;k<4;++k)Add(input,63,8,.2f);C(Oracle(m,input,64,false)==0);}
 return 0;
}
static int Assets(Ogre::Mesh*oracle){
 const char*paths[]={"human/human_male.mesh","human/human_female.mesh","stick_person/Stick_person.mesh","stick_person/Hive_Queen.mesh","robot/robot.mesh"};
 unsigned accepted=0,rejected=0;
 for(unsigned k=0;k<5;++k){std::string path="D:/SteamLibrary/steamapps/common/Kenshi/data/character/meshes/";path+=paths[k];std::ifstream*file=new std::ifstream(path.c_str(),std::ios::binary);C(file->good());Ogre::DataStreamPtr stream(OGRE_NEW Ogre::FileStreamDataStream(path,file,false));char name[40];sprintf_s(name,"weights-offline-%u",k);Ogre::MeshPtr mesh=Ogre::MeshManager::getSingleton().createManual(name,"General",NULL);Ogre::MeshSerializer serializer;serializer.importMesh(stream,mesh.get());
  for(unsigned sub=0;sub<mesh->getNumSubMeshes();++sub){Ogre::SubMesh*s=mesh->getSubMesh((unsigned short)sub);if(!s->vertexData||s->getBoneAssignments().empty())continue;List input(s->getBoneAssignments()),trial(input);unsigned short maximum=0;bool yes=mesh_weights::Try(s->vertexData->vertexCount,trial,&maximum);if(yes)++accepted;else ++rejected;C(Oracle(oracle,input,s->vertexData->vertexCount,yes)==0);printf("ASSET %s sub=%u vertices=%u assignments=%u accepted=%u\n",paths[k],sub,(unsigned)s->vertexData->vertexCount,(unsigned)input.size(),yes?1:0);}
  stream.setNull();delete file;
 }
 printf("ASSET TOTAL accepted=%u rejected=%u\n",accepted,rejected);C(accepted>0);return 0;
}
static int Benchmark(Ogre::Mesh*m){
 LARGE_INTEGER freq;QueryPerformanceFrequency(&freq);
 for(unsigned bones=1;bones<=4;++bones)for(unsigned cold=0;cold<2;++cold){List input;const unsigned vertices=16000;for(unsigned v=0;v<vertices;++v)for(unsigned k=0;k<bones;++k)Add(input,v,k,cold?.17f*(k+1):1.0f/bones);double us[2]={};unsigned short result=0;
  for(unsigned round=0;round<20;++round)for(unsigned order=0;order<2;++order){unsigned path=(order+round)%2;List data(input);LARGE_INTEGER start,end;QueryPerformanceCounter(&start);if(path)C(mesh_weights::Try(vertices,data,&result));else result=m->_rationaliseBoneAssignments(vertices,data);QueryPerformanceCounter(&end);us[path]+=double(end.QuadPart-start.QuadPart)*1e6/freq.QuadPart/20;}
  printf("BENCH vertices=%u bones=%u normalize=%u native_us=%.2f linear_us=%.2f ratio=%.3f max=%u\n",vertices,bones,cold,us[0],us[1],us[1]/us[0],result);
 }
 return 0;
}
int main(int argc,char**argv){setvbuf(stdout,NULL,_IONBF,0);Ogre::Root root("","","");Ogre::DefaultHardwareBufferManager hardware;Ogre::MeshPtr mesh=Ogre::MeshManager::getSingleton().createManual("weight-oracle","General",NULL);C(Cases(mesh.get())==0);C(Integration(mesh.get())==0);if(argc>1&&!strcmp(argv[1],"assets"))C(Assets(mesh.get())==0);else if(argc>1)C(Benchmark(mesh.get())==0);mesh.setNull();Ogre::MeshManager::getSingleton().removeAll();printf("PASS %u native bone weight checks\n",checks);return 0;}
