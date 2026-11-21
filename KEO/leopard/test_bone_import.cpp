#define NOMINMAX
#include <Windows.h>
#include <stdint.h>
#include <ogre/OgreRoot.h>
#include <ogre/OgreMesh.h>
#include <ogre/OgreSubMesh.h>
#include <ogre/OgreException.h>
#include <ogre/OgreMeshManager.h>
#include <ogre/OgreMeshSerializer.h>
#include <ogre/OgreDefaultHardwareBufferManager.h>
#include <ogre/OgreDataStream.h>
#include <xmmintrin.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <stdexcept>
#include <fstream>
#include "fixes/near_page.h"
#include "fixes/code_patch_batch.h"
#include "fixes/world/bone_import.h"
namespace fixes{struct Config{bool meshWeightsEnabled;};static Config g_fixesCfg={true};}
static DWORD ownerThread=GetCurrentThreadId();
static bool IsMainThread(){return GetCurrentThreadId()==ownerThread;}
static bool imageIdEnabled=true;
static bool ReadModuleImageId(HMODULE module,DWORD*stamp,DWORD*size){if(!imageIdEnabled)return false;IMAGE_DOS_HEADER*dos=(IMAGE_DOS_HEADER*)module;IMAGE_NT_HEADERS64*nt=(IMAGE_NT_HEADERS64*)((char*)module+dos->e_lfanew);*stamp=nt->FileHeader.TimeDateStamp;*size=nt->OptionalHeader.SizeOfImage;return true;}
#define KEO_BONE_IMPORT_TEST
#include "../src/fixes/world/bone_import_patch.cpp"
static unsigned checks=0;
#define C(x) do{++checks;if(!(x)){printf("FAIL line %u: %s\n",__LINE__,#x);return 1;}}while(0)
typedef Ogre::SubMesh::VertexBoneAssignmentList List;
struct Access:Ogre::SubMesh{using Ogre::SubMesh::mBoneAssignments;using Ogre::SubMesh::mBoneAssignmentsOutOfDate;};
static_assert(offsetof(Access,mBoneAssignments)==0xd0&&offsetof(Access,mBoneAssignmentsOutOfDate)==0xf8,"native SubMesh layout");
static_assert(sizeof(List)==40&&sizeof(Ogre::VertexBoneAssignment)==12,"native assignment ABI");
static bone_import::Original native=NULL;
static bone_import::Allocate allocate=NULL;
static bone_import::Link link=NULL;
static unsigned originalCalls=0,allocations=0,links=0;
static unsigned reentry=0;static Access*other=NULL;static void*capturedNode=NULL;
typedef void(*Release)(void*,void*,uintptr_t);static Release release=NULL;static unsigned freed=0;
static void CountRelease(void*pool,void*block,uintptr_t arg){if(block==capturedNode)++freed;release(pool,block,arg);}
static bool assetFast=false;static unsigned assetCalls=0,assetUnordered=0;
static void TrackAssignment(void*self,const void*value){
 ++assetCalls;const Ogre::VertexBoneAssignment*v=(const Ogre::VertexBoneAssignment*)value;List&list=((Access*)self)->mBoneAssignments;
 if(!list.empty()&&v->vertexIndex<list.rbegin()->first)++assetUnordered;
 if(assetFast)bone_import::Add(self,value,native,allocate,link,true,true);else native(self,value);
}
static void Original(void*self,const void*v){++originalCalls;native(self,v);}
static void*Allocate(void*map,const void*v){++allocations;
 Access*self=(Access*)((char*)map-0xd0);
 if(reentry==1||reentry==7){Ogre::VertexBoneAssignment b;memset(&b,0xa5,sizeof(b));b.vertexIndex=100;b.boneIndex=77;b.weight=.5f;native(self,&b);}
 if(reentry==6){Ogre::VertexBoneAssignment b;memset(&b,0xa5,sizeof(b));b.vertexIndex=5;b.boneIndex=77;b.weight=.5f;native(self,&b);}
 if(reentry==2)self->clearBoneAssignments();
 if(reentry==3)self->mBoneAssignments.swap(other->mBoneAssignments);
 if(reentry==4)throw std::bad_alloc();
 capturedNode=allocate(map,v);if(reentry==5)*(uintptr_t*)((char*)map+0x10)=~uintptr_t(0)-1;return capturedNode;
}
static void*Link(void*map,void**out,bool left,void*parent,void*node){++links;return link(map,out,left,parent,node);}
static Ogre::VertexBoneAssignment Value(unsigned vertex,unsigned bone,unsigned bits){Ogre::VertexBoneAssignment b;memset(&b,0xa5,sizeof(b));b.vertexIndex=vertex;b.boneIndex=(unsigned short)bone;memcpy(&b.weight,&bits,4);return b;}
static bool Same(List&a,List&b){if(a.size()!=b.size())return false;List::iterator x=a.begin(),y=b.begin();for(;x!=a.end();++x,++y)if(x->first!=y->first||memcmp(&x->second,&y->second,12))return false;return true;}
#include "bone_import_native/fixture_hooks.h"
static int Oracle(const std::vector<Ogre::VertexBoneAssignment>&values,bool enabled=true,bool owner=true){
 Access expected,actual;expected.useSharedVertices=actual.useSharedVertices=false;expected.mBoneAssignmentsOutOfDate=actual.mBoneAssignmentsOutOfDate=false;originalCalls=allocations=links=0;
 const unsigned saved=_mm_getcsr();unsigned expectedFast=0,largest=0;
 for(size_t i=0;i<values.size();++i){if(enabled&&owner&&(!i||values[i].vertexIndex>=largest))++expectedFast;if(values[i].vertexIndex>largest)largest=values[i].vertexIndex;native(&expected,&values[i]);unsigned control=0x1f80|unsigned(i%4)<<13;_mm_setcsr(control);bone_import::Add(&actual,&values[i],Original,Allocate,Link,enabled,owner);C(_mm_getcsr()==control);C(Same(expected.mBoneAssignments,actual.mBoneAssignments));C(actual.mBoneAssignmentsOutOfDate);}
 _mm_setcsr(saved);C(originalCalls==values.size()-expectedFast);C(allocations==expectedFast);C(links==allocations);return 0;
}
static int Cases(){
 std::vector<Ogre::VertexBoneAssignment> values;
 for(unsigned v=0;v<256;++v)for(unsigned k=0;k<v%4+1;++k)values.push_back(Value(v,k,0x3e2e147b));C(Oracle(values)==0);C(Oracle(values,false)==0);C(Oracle(values,true,false)==0);
 values.clear();for(unsigned v=0;v<256;++v)values.push_back(Value(255-v,v%11,0x3f000000));C(Oracle(values)==0);
 values.clear();unsigned random=17;for(unsigned v=0;v<512;++v){random=random*1664525+1013904223;values.push_back(Value(random%73,v,random));}C(Oracle(values)==0);
 values.clear();const unsigned bits[]={0,0x80000000,1,0x7f800000,0xff800000,0x7fc12345,0x7f812345,0xbf000000};for(unsigned i=0;i<8;++i)values.push_back(Value(i?0xffffffffu:0,i,bits[i]));C(Oracle(values)==0);
 return 0;
}
static int Integration(HMODULE module){
 C(bone_import_patch::Admit(module)==(char*)module+0x369330);imageIdEnabled=false;C(!bone_import_patch::Admit(module));imageIdEnabled=true;C(!bone_import_patch::Admit(NULL));DWORD saved=ownerThread;ownerThread=0;C(!bone_import_patch::Admit(module));ownerThread=saved;
 std::vector<unsigned char>image(0x9c9000,0);for(unsigned k=0;k<sizeof(bone_import_patch::proofs)/sizeof(bone_import_patch::proofs[0]);++k){const bone_import_patch::Proof&p=bone_import_patch::proofs[k];memcpy(&image[p.rva],p.bytes,p.size);}C(bone_import_patch::Validate(&image[0],image.size()));C(!bone_import_patch::Validate(&image[0],image.size()-1));
 for(unsigned k=0;k<sizeof(bone_import_patch::proofs)/sizeof(bone_import_patch::proofs[0]);++k){const bone_import_patch::Proof&p=bone_import_patch::proofs[k];for(unsigned j=0;j<p.size;++j){image[p.rva+j]^=1;C(!bone_import_patch::Validate(&image[0],image.size()));image[p.rva+j]^=1;}}
 bone_import_patch::original=Original;bone_import_patch::allocate=Allocate;bone_import_patch::link=Link;
 for(unsigned mode=0;mode<4;++mode){Access actual;actual.useSharedVertices=false;Ogre::VertexBoneAssignment value=Value(1,7,0x3f000000);originalCalls=allocations=0;bone_import_patch::admitted=mode!=1;fixes::g_fixesCfg.meshWeightsEnabled=mode!=2;if(mode==3)ownerThread=0;bone_import_patch::Import(&actual,&value);C(actual.mBoneAssignments.size()==1);C(originalCalls==(mode?1u:0u));C(allocations==(mode?0u:1u));ownerThread=saved;}
 bone_import_patch::admitted=true;fixes::g_fixesCfg.meshWeightsEnabled=true;return 0;
}
static int Reentry(){
 const unsigned modes[]={1,2,3,6,7};for(unsigned modeIndex=0;modeIndex<5;++modeIndex){unsigned mode=modes[modeIndex];Access expected,actual,extra;expected.useSharedVertices=actual.useSharedVertices=extra.useSharedVertices=false;Ogre::VertexBoneAssignment first=Value(mode==7?5:1,2,0x3f000000),inserted=Value(5,3,0x3f000000),last=Value(mode==6?5:100,77,0x3f000000);native(&actual,&first);native(&expected,&first);native(&extra,&last);reentry=mode;other=&extra;
  const void*input=&inserted;
  if(mode==1||mode==6||mode==7)native(&expected,&last);
  if(mode==2){inserted=actual.mBoneAssignments.begin()->second;input=&actual.mBoneAssignments.begin()->second;expected.clearBoneAssignments();}
  if(mode==3){expected.clearBoneAssignments();native(&expected,&last);}
  native(&expected,&inserted);bone_import::Add(&actual,input,Original,Allocate,Link,true,true);C(Same(expected.mBoneAssignments,actual.mBoneAssignments));C(actual.mBoneAssignmentsOutOfDate);if(mode==3){C(extra.mBoneAssignments.size()==1&&extra.mBoneAssignments.begin()->first==1);}reentry=0;other=NULL;
 }
 {Access actual;actual.useSharedVertices=false;actual.mBoneAssignmentsOutOfDate=false;Ogre::VertexBoneAssignment v=Value(1,3,0x3f000000);reentry=4;capturedNode=NULL;bool caught=false;try{bone_import::Add(&actual,&v,Original,Allocate,Link,true,true);}catch(const std::bad_alloc&){caught=true;}reentry=0;C(caught&&!capturedNode&&actual.mBoneAssignments.empty()&&!actual.mBoneAssignmentsOutOfDate);}
 {Access actual;actual.useSharedVertices=true;actual.mBoneAssignmentsOutOfDate=false;Ogre::VertexBoneAssignment v=Value(1,3,0x3f000000);originalCalls=allocations=links=0;bool caught=false;try{bone_import::Add(&actual,&v,Original,Allocate,Link,true,true);}catch(const Ogre::Exception&){caught=true;}C(caught&&originalCalls==1&&allocations==0&&links==0&&actual.mBoneAssignments.empty()&&!actual.mBoneAssignmentsOutOfDate);}
 {Access actual;actual.useSharedVertices=false;Ogre::VertexBoneAssignment seed=Value(1,3,0x3f000000),v=Value(2,4,0x3f000000);native(&actual,&seed);actual.mBoneAssignmentsOutOfDate=false;freed=0;capturedNode=NULL;reentry=5;bool caught=false;try{bone_import::Add(&actual,&v,Original,Allocate,Link,true,true);}catch(const std::length_error&){caught=true;}reentry=0;uintptr_t&count=*(uintptr_t*)((char*)&actual.mBoneAssignments+0x10);const uintptr_t observedCount=count;const bool observedDirty=actual.mBoneAssignmentsOutOfDate;count=1;C(caught&&freed==1&&capturedNode&&observedCount==~uintptr_t(0)-1&&!observedDirty);C(actual.mBoneAssignments.begin()->first==1);}
 return 0;
}
static int Assets(){
 const char*names[]={"human/human_male.mesh","human/human_female.mesh","stick_person/Stick_person.mesh","stick_person/Hive_Queen.mesh","robot/robot.mesh"};
 for(unsigned asset=0;asset<5;++asset){Ogre::MeshPtr meshes[2];unsigned calls[2]={},unordered[2]={};for(unsigned pass=0;pass<2;++pass){assetFast=pass!=0;assetCalls=assetUnordered=0;std::string path="D:/SteamLibrary/steamapps/common/Kenshi/data/character/meshes/";path+=names[asset];std::ifstream*file=new std::ifstream(path.c_str(),std::ios::binary);C(file->good());Ogre::DataStreamPtr stream(OGRE_NEW Ogre::FileStreamDataStream(path,file,false));char tag[60];sprintf_s(tag,"bone-import-%u-%u",asset,pass);meshes[pass]=Ogre::MeshManager::getSingleton().createManual(tag,"General",NULL);Ogre::MeshSerializer serializer;serializer.importMesh(stream,meshes[pass].get());stream.setNull();delete file;calls[pass]=assetCalls;unordered[pass]=assetUnordered;}
  C(calls[0]>0&&calls[0]==calls[1]&&unordered[0]==unordered[1]);C(meshes[0]->getNumSubMeshes()==meshes[1]->getNumSubMeshes());for(unsigned sub=0;sub<meshes[0]->getNumSubMeshes();++sub){List&a=const_cast<List&>(meshes[0]->getSubMesh((unsigned short)sub)->getBoneAssignments());List&b=const_cast<List&>(meshes[1]->getSubMesh((unsigned short)sub)->getBoneAssignments());C(Same(a,b));}printf("ASSET %s assignments=%u unordered=%u bitwise=PASS\n",names[asset],calls[0],unordered[0]);meshes[0].setNull();meshes[1].setNull();Ogre::MeshManager::getSingleton().removeAll();
 }
 assetFast=false;return 0;
}
static int Benchmark(){
 bone_import_patch::original=native;bone_import_patch::allocate=allocate;bone_import_patch::link=link;bone_import_patch::admitted=true;fixes::g_fixesCfg.meshWeightsEnabled=true;
 LARGE_INTEGER freq;QueryPerformanceFrequency(&freq);
 for(unsigned pattern=0;pattern<3;++pattern){std::vector<Ogre::VertexBoneAssignment>values;unsigned random=13;const unsigned count=32000;for(unsigned i=0;i<count;++i){random=random*1664525+1013904223;unsigned key=pattern==0?i/2:pattern==1?(count-i)/2:random%16000;values.push_back(Value(key,i%255,0x3f000000));}double elapsed[2]={};
  for(unsigned repeat=0;repeat<12;++repeat)for(unsigned order=0;order<2;++order){unsigned fast=(order+repeat)%2;Access sub;sub.useSharedVertices=false;LARGE_INTEGER begin,end;QueryPerformanceCounter(&begin);for(unsigned i=0;i<count;++i){if(fast)bone_import_patch::Import(&sub,&values[i]);else native(&sub,&values[i]);}QueryPerformanceCounter(&end);elapsed[fast]+=double(end.QuadPart-begin.QuadPart)*1e6/freq.QuadPart/12;C(sub.mBoneAssignments.size()==count);}
  printf("BENCH pattern=%u assignments=%u native_us=%.2f optimized_us=%.2f ratio=%.3f\n",pattern,count,elapsed[0],elapsed[1],elapsed[1]/elapsed[0]);
 }
 return 0;
}
int main(int argc,char**argv){setvbuf(stdout,NULL,_IONBF,0);Ogre::Root root("","","");Ogre::DefaultHardwareBufferManager hardware;HMODULE dll=GetModuleHandleA("OgreMain_x64.dll");C(dll!=NULL);native=(bone_import::Original)((char*)dll+0x369330);allocate=(bone_import::Allocate)((char*)dll+0x1bb050);link=(bone_import::Link)((char*)dll+0x1ba270);release=(Release)((char*)dll+0x1a1340);C(Integration(dll)==0);FixtureHooks hooks;C(hooks.Arm(dll)==0);C(!bone_import_patch::Admit(dll));C(Cases()==0);C(Reentry()==0);if(argc>1&&!strcmp(argv[1],"assets"))C(Assets()==0);else if(argc>1)C(Benchmark()==0);C(hooks.Restore()==0);C(bone_import_patch::Admit(dll)!=NULL);printf("PASS %u native bone import checks\n",checks);return 0;}
