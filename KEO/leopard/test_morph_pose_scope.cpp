// PRIVATE actual-Ogre fixture; no game executable/process access.
#define NOMINMAX
#include <windows.h>
#include <wincrypt.h>
#include <ogre/OgreRoot.h>
#include <ogre/OgreMeshManager.h>
#include <ogre/OgreMesh.h>
#include <ogre/OgrePose.h>
#include <ogre/OgreSubMesh.h>
#include <ogre/OgreDefaultHardwareBufferManager.h>
#include <ogre/OgreVertexIndexData.h>
#include <cstdio>
#include <vector>
#include <string>
#include <cstring>
static unsigned checks=0;
typedef int(*SdkAdd)(void*,void*,void**);static SdkAdd sdkAdd=NULL;
#define C(x) do{++checks;if(!(x)){std::printf("CHECK line%d %s\n",__LINE__,#x);return 1;}}while(0)

#include <ogre/OgreException.h>
#include <ogre/OgreMaterialManager.h>
#include <ogre/OgreSceneManager.h>
#include <ogre/OgreEntity.h>
#include <stdexcept>
static bool owner=true,verifiedGog=false;static unsigned errors=0;
bool NativeProfileIsGog(){return verifiedGog;}
bool IsMainThread();
namespace fixes {struct Config{bool morphPoseReuseEnabled;};Config g_fixesCfg={true};}
void LogError(const std::string&){++errors;}
#define KEO_DETAIL(x) ((void)0)
static HMODULE fakeGame=NULL;static bool fakeHooks=false;static unsigned hookCalls=0,failHook=0,nullHook=0;
static Ogre::Pose*DummyPose(const Ogre::Pose*){return NULL;}static void DummyRemove(Ogre::Mesh*){}
namespace KenshiLib {enum Status{SUCCESS,FAILURE};Status AddHook(void*t,void*h,void**out){++hookCalls;
 if(fakeHooks){*out=hookCalls==nullHook?NULL:hookCalls==1?(void*)DummyPose:(void*)DummyRemove;return hookCalls==failHook?FAILURE:SUCCESS;}
 return (Status)sdkAdd(t,h,out);}}
#define KEO_MORPH_GAME_MODULE() fakeGame
bool ReadModuleImageId(HMODULE m,DWORD*stamp,DWORD*size){
 const IMAGE_DOS_HEADER*d=(const IMAGE_DOS_HEADER*)m;if(d->e_magic!=IMAGE_DOS_SIGNATURE||d->e_lfanew<0||d->e_lfanew>1024*1024)return false;
 const IMAGE_NT_HEADERS64*n=(const IMAGE_NT_HEADERS64*)((const char*)m+d->e_lfanew);
 if(n->Signature!=IMAGE_NT_SIGNATURE||n->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64)return false;
 *stamp=n->FileHeader.TimeDateStamp;*size=n->OptionalHeader.SizeOfImage;return true;
}
#define KEO_MORPH_POSE_SCOPE_TEST
#ifndef KEO_MORPH_POSE_SOURCE
#define KEO_MORPH_POSE_SOURCE "../src/fixes/world/morph_pose_scope.cpp"
#endif
#include KEO_MORPH_POSE_SOURCE
static bool DiskHash(HMODULE module,const char*expected){
 char path[MAX_PATH];if(!GetModuleFileNameA(module,path,MAX_PATH))return false;
 HANDLE f=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);if(f==INVALID_HANDLE_VALUE)return false;
 HCRYPTPROV provider=0;HCRYPTHASH hash=0;bool ok=CryptAcquireContextA(&provider,NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)!=0;
 if(ok)ok=CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)!=0;
 unsigned char buf[8192];DWORD got=0;
 while(ok){if(!ReadFile(f,buf,sizeof(buf),&got,NULL)){ok=false;break;}if(!got)break;ok=CryptHashData(hash,buf,got,0)!=0;}
 unsigned char digest[32];DWORD n=sizeof(digest);if(ok)ok=CryptGetHashParam(hash,HP_HASHVAL,digest,&n,0)!=0&&n==32;
 char hex[65];if(ok){for(unsigned i=0;i<32;++i)sprintf_s(hex+i*2,3,"%02x",digest[i]);ok=strcmp(hex,expected)==0;}
 if(hash)CryptDestroyHash(hash);if(provider)CryptReleaseContext(provider,0);CloseHandle(f);return ok;
}
static int InitSdk(){
 HMODULE sdk=LoadLibraryA("KenshiLib.dll");C(sdk!=NULL);
 C(DiskHash(sdk,"5b6240622d06801dc39896e385efb834b8e1fe2fcdf86067d071ae7a04f6c83b"));
 static const unsigned char prefix[]={0x40,0x53,0x48,0x83,0xec,0x60,0x48,0x8b,0x05,0xff,0x15,0x0d,0x00,0x48,0x33,0xc4,0x48,0x89,0x44,0x24,0x58,0x33,0xdb,0x48,0x39,0x1d,0x86,0x2b,0x0d,0x00,0x74,0x08};
 C(memcmp((char*)sdk+0x14884,prefix,sizeof(prefix))==0);
 C(*(unsigned char*)((char*)sdk+0x227f)==0xe8);
 int displacement=0;memcpy(&displacement,(char*)sdk+0x2280,4);C(0x227f+5+displacement==0x14884);
 sdkAdd=(SdkAdd)GetProcAddress(sdk,"?AddHook@KenshiLib@@YA?AW4HookStatus@1@PEAX0PEAPEAX@Z");C(sdkAdd!=NULL);
 // Private test-only MinHook initialization; never InitRVAs or game initialization.
 typedef int(*Init)();C(((Init)((char*)sdk+0x14884))()==0);return 0;
}
struct Loader:Ogre::ManualResourceLoader{void loadResource(Ogre::Resource*){}};
static Loader loader;static unsigned serial=0;
static bool separateNormals=false;static unsigned weightMode=0;
static Ogre::VertexData*Vertices(unsigned seed){
 Ogre::VertexData*v=OGRE_NEW Ogre::VertexData;v->vertexStart=0;v->vertexCount=4;
 v->vertexDeclaration->addElement(0,0,Ogre::VET_FLOAT3,Ogre::VES_POSITION);
 v->vertexDeclaration->addElement(separateNormals?1:0,separateNormals?0:12,Ogre::VET_FLOAT3,Ogre::VES_NORMAL);
 unsigned stride=separateNormals?12:24;Ogre::HardwareVertexBufferSharedPtr b=Ogre::HardwareBufferManager::getSingleton().createVertexBuffer(stride,4,Ogre::HardwareBuffer::HBU_STATIC_WRITE_ONLY,false);
 float data[24];for(unsigned i=0;i<4;++i){unsigned k=i*(stride/4);for(unsigned j=0;j<3;++j)data[k+j]=float(seed*13+i*3+j)/7.0f;if(!separateNormals){data[k+3]=.2f;data[k+4]=.3f;data[k+5]=.4f;}}
 b->writeData(0,stride*4,data,true);v->vertexBufferBinding->setBinding(0,b);
 if(separateNormals){Ogre::HardwareVertexBufferSharedPtr n=Ogre::HardwareBufferManager::getSingleton().createVertexBuffer(12,4,Ogre::HardwareBuffer::HBU_STATIC_WRITE_ONLY,false);float normals[12];for(unsigned i=0;i<12;++i)normals[i]=float(i%3+1)/5;n->writeData(0,sizeof(normals),normals,true);v->vertexBufferBinding->setBinding(1,n);}
 return v;
}
static Ogre::MeshPtr Model(unsigned subs,unsigned poses,bool loaded=true){
 char name[64];sprintf_s(name,"fixture-source-%u",++serial);Ogre::MeshPtr m=Ogre::MeshManager::getSingleton().createManual(name,"General",&loader);
 m->sharedVertexData=Vertices(1);m->_setBounds(Ogre::AxisAlignedBox(-1,-2,-3,4,5,6),false);m->_setBoundingSphereRadius(9.5f);
 for(unsigned i=0;i<subs;++i){char subname[64];sprintf_s(subname,"sub-%u",i);Ogre::SubMesh*s=m->createSubMesh(subname);
  s->useSharedVertices=i%2==0;if(!s->useSharedVertices)s->vertexData=Vertices(i+3);
  s->operationType=Ogre::RenderOperation::OT_TRIANGLE_LIST;s->setMaterialName("BaseWhite");
  s->indexData->indexStart=0;s->indexData->indexCount=6;s->indexData->indexBuffer=Ogre::HardwareBufferManager::getSingleton().createIndexBuffer(Ogre::HardwareIndexBuffer::IT_16BIT,6,Ogre::HardwareBuffer::HBU_STATIC_WRITE_ONLY,false);
  unsigned short indices[]={0,1,2,2,3,0};s->indexData->indexBuffer->writeData(0,sizeof(indices),indices,true);
  s->extremityPoints.push_back(Ogre::Vector3(float(i),2,3));
 }
 for(unsigned i=0;i<poses;++i){char pose[64];sprintf_s(pose,"pose-%u-long-name-to-exercise-owned-string",i);Ogre::Pose*p=m->createPose(0,pose);
  for(unsigned v=0;v<3;++v){Ogre::Vector3 o(float(i)/11,float(v)/7,-.25f);if(i%2)p->addVertex(v,o,Ogre::Vector3(.1f,float(i+v)/13,.9f));else p->addVertex(v,o);}
 }
 if(loaded)m->load();return m;
}


static int AdmissionCases(HMODULE ogre){
 fakeGame=(HMODULE)VirtualAlloc(NULL,0x232c000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);C(fakeGame!=NULL);
 IMAGE_DOS_HEADER*dos=(IMAGE_DOS_HEADER*)fakeGame;dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=0x100;
 IMAGE_NT_HEADERS64*nt=(IMAGE_NT_HEADERS64*)((char*)fakeGame+0x100);nt->Signature=IMAGE_NT_SIGNATURE;nt->FileHeader.Machine=IMAGE_FILE_MACHINE_AMD64;nt->FileHeader.TimeDateStamp=0x65d604d7;nt->OptionalHeader.SizeOfImage=0x232c000;
 for(size_t i=0;i<sizeof(morph_pose_scope::gameProofs)/sizeof(morph_pose_scope::gameProofs[0]);++i){const morph_pose_scope::Proof&p=morph_pose_scope::gameProofs[i];memcpy((char*)fakeGame+p.rva,p.bytes,p.size);}
 C(morph_pose_scope::Admit(ogre,fakeGame));C(!morph_pose_scope::Admit(NULL,fakeGame));C(!morph_pose_scope::Admit(ogre,NULL));C(!morph_pose_scope::Admit((HMODULE)1,fakeGame));C(!morph_pose_scope::Admit(ogre,(HMODULE)1));
 for(size_t i=0;i<sizeof(morph_pose_scope::gameProofs)/sizeof(morph_pose_scope::gameProofs[0]);++i){uintptr_t r=morph_pose_scope::gameProofs[i].rva;((char*)fakeGame)[r]^=1;C(!morph_pose_scope::Admit(ogre,fakeGame));((char*)fakeGame)[r]^=1;}
 nt->FileHeader.TimeDateStamp^=1;C(!morph_pose_scope::Admit(ogre,fakeGame));nt->FileHeader.TimeDateStamp^=1;nt->OptionalHeader.SizeOfImage^=1;C(!morph_pose_scope::Admit(ogre,fakeGame));nt->OptionalHeader.SizeOfImage^=1;
 // Authenticating the executable does not bypass exact Ogre or GOG caller bytes.
 nt->FileHeader.TimeDateStamp=0x65d60519;nt->OptionalHeader.SizeOfImage=0x232a000;
 for(size_t i=0;i<sizeof(morph_pose_scope::gogGameProofs)/sizeof(morph_pose_scope::gogGameProofs[0]);++i){
  const morph_pose_scope::Proof&p=morph_pose_scope::gogGameProofs[i];memcpy((char*)fakeGame+p.rva,p.bytes,p.size);
 }
 C(!morph_pose_scope::Admit(ogre,fakeGame));verifiedGog=true;C(morph_pose_scope::Admit(ogre,fakeGame));
 for(size_t i=0;i<sizeof(morph_pose_scope::gogGameProofs)/sizeof(morph_pose_scope::gogGameProofs[0]);++i){
  uintptr_t r=morph_pose_scope::gogGameProofs[i].rva;((char*)fakeGame)[r]^=1;
  C(!morph_pose_scope::Admit(ogre,fakeGame));((char*)fakeGame)[r]^=1;
 }
 nt->FileHeader.TimeDateStamp=0x6602d5e3;nt->OptionalHeader.SizeOfImage=0x232c000;C(!morph_pose_scope::Admit(ogre,fakeGame));
 verifiedGog=false;nt->FileHeader.TimeDateStamp=0x65d604d7;
 for(size_t i=0;i<sizeof(morph_pose_scope::gameProofs)/sizeof(morph_pose_scope::gameProofs[0]);++i){
  const morph_pose_scope::Proof&p=morph_pose_scope::gameProofs[i];memcpy((char*)fakeGame+p.rva,p.bytes,p.size);
 }
 fakeHooks=true;morph_pose_scope::SetCloneHooksReady(true);morph_pose_scope::SetCreateHookReady(true);
 for(unsigned fault=0;fault<5;++fault){hookCalls=0;failHook=fault<3?fault:0;nullHook=fault>=3?fault-2:0;
  morph_pose_scope::tried=morph_pose_scope::admitted=false;morph_pose_scope::originalPose=NULL;morph_pose_scope::originalRemove=NULL;
  morph_pose_scope::Install(NULL,NULL);C(morph_pose_scope::tried&&morph_pose_scope::admitted==(fault==0));C(morph_pose_scope::cloneReady&&morph_pose_scope::createReady);C(hookCalls==((fault==1||fault==3)?1u:2u));
  if(fault==1||fault==2)C(morph_pose_scope::originalPose!=NULL);if(fault==2)C(morph_pose_scope::originalRemove!=NULL);unsigned before=hookCalls;morph_pose_scope::Install(NULL,NULL);C(before==hookCalls);
 }
 fakeHooks=false;failHook=nullHook=hookCalls=0;morph_pose_scope::tried=morph_pose_scope::admitted=false;morph_pose_scope::originalPose=NULL;morph_pose_scope::originalRemove=NULL;return 0;
}
typedef void*(*ManagerCreate)(void*,void*,const void*,const void*,void*);
static bool wrongCloneCaller=false,wrongCaptureCaller=false,wrongCaptureResult=false,injectNested=false,nestedMaskOk=false;
static morph_pose_scope::CloneFn nativeClone=NULL;static ManagerCreate nativeCreate=NULL;
static morph_pose_scope::PoseFn nativePose=NULL;static unsigned deepClones=0;
static unsigned rejectAfter=0;static bool deepFault=false;static Ogre::Mesh*faultSource=NULL;
bool IsMainThread(){return owner&&!(rejectAfter&&morph_pose_scope::cloning&&morph_pose_scope::cloning->count>=rejectAfter);}
static Ogre::Pose*DeepClone(const Ogre::Pose*p){++deepClones;if(deepFault){faultSource->getSubMesh(65535);}return nativePose(p);}
static void*CloneBridge(void*self,void*out,const void*name,const void*group){return morph_pose_scope::Clone(nativeClone,self,out,name,group,morph_pose_scope::gameBase+0x71ae7+(wrongCloneCaller?1:0));}
static void*CreateBridge(void*self,void*out,const void*name,const void*group,void*load){
 uintptr_t caller=(uintptr_t)_ReturnAddress();void*result=nativeCreate(self,out,name,group,load);
 if(injectNested&&morph_pose_scope::cloning){injectNested=false;morph_pose_scope::Context*parent=morph_pose_scope::cloning;Ogre::MeshPtr nested;std::string nestedName="nested-masked-clone";
  morph_pose_scope::Clone(nativeClone,parent->Source().get(),&nested,&nestedName,group,0);
  nestedMaskOk=morph_pose_scope::cloning==parent&&nested->getPose(0)!=parent->Source()->getPose(0);nested->removeAllPoses();
 }
 morph_pose_scope::CaptureCloneResult(wrongCaptureResult?(char*)result+16:result,out,caller+(wrongCaptureCaller?1:0));return result;
}
typedef void(*RemoveBoundaryFn)(Ogre::Mesh*);
static uintptr_t boundaryReturn=0;static volatile size_t afterBoundary=0;
static void ObserveBoundary(Ogre::Mesh*){boundaryReturn=(uintptr_t)_ReturnAddress();}
static RemoveBoundaryFn boundaryFunction=ObserveBoundary;
static __declspec(noinline) void BoundaryRemove(Ogre::Mesh*m){boundaryFunction(m);afterBoundary=m->getPoseCount();}
static bool unsafeRemoval=false;static unsigned keptAtRemove=0;static morph_pose_scope::RemoveFn actualRemove=NULL;
static void RemoveSpy(Ogre::Mesh*m){
 // Negative controls must fail CHECK rather than corrupt native source maps.
 // Observe before native deletion; canonical cleanup leaves this guard idle.
 Ogre::PoseList&list=morph_pose_scope::Poses(m);size_t write=0;
 for(size_t i=0;i<list.size();++i){bool borrowed=false;
  for(morph_pose_scope::Context*c=morph_pose_scope::tracked;c;c=c->previous)
   for(size_t j=0;j<c->count;++j)if(list[i]==c->borrowed[j])borrowed=true;
  if(borrowed)unsafeRemoval=true;else list[write++]=list[i];
 }
 list.resize(write);keptAtRemove=(unsigned)m->getPoseCount();actualRemove(m);
}
static Ogre::MeshPtr held;static unsigned mode=0;static bool borrowedSeen=false,boundaryRefsRestored=false,pinIdentityValid=false;
static std::vector<unsigned char>Bytes(Ogre::Mesh*m){
 const Ogre::VertexBufferBinding::VertexBufferBindingMap&bindings=m->sharedVertexData->vertexBufferBinding->getBindings();std::vector<unsigned char>v;
 for(Ogre::VertexBufferBinding::VertexBufferBindingMap::const_iterator i=bindings.begin();i!=bindings.end();++i){size_t offset=v.size(),bytes=i->second->getSizeInBytes();v.resize(offset+bytes);i->second->readData(0,bytes,&v[offset]);}return v;
}
static void Blend(Ogre::Mesh*m){for(size_t i=0;i<m->getPoseCount();++i){Ogre::Pose*p=m->getPose((unsigned short)i);float weight=float(i+1)/13;
 if(weightMode){if(i%4==0)weight=0;else if(i%4==1)weight=-.5f;else if(i%4==2){unsigned bits=0x7fc12345;memcpy(&weight,&bits,4);}}
 Ogre::Mesh::softwareVertexPoseBlend(weight,p->getVertexOffsets(),p->getNormals(),m->sharedVertexData);
}}
static Ogre::SceneManager*activeScene=NULL;static Ogre::MeshPtr nestedHeld;static bool nestedSafe=false;
static void*NestedRemove(const void*,void*entity,void*,bool){
 Ogre::Entity*e=static_cast<Ogre::Entity*>(entity);unsigned refs=e->getMesh().useCount();
 held->removeAllPoses();nestedSafe=held->getPoseCount()==0&&e->getMesh().useCount()==refs&&morph_pose_scope::current&&!morph_pose_scope::current->enabled;return entity;
}
static void*NestedTarget(const void*name,void*entity,void*,bool){
 Ogre::Entity*e=static_cast<Ogre::Entity*>(entity);nestedHeld=e->getMesh()->clone(*static_cast<const std::string*>(name),"General");
 nestedSafe=!morph_pose_scope::current->enabled&&nestedHeld->getPose(0)!=e->getMesh()->getPose(0);nestedHeld->removeAllPoses();return entity;
}
static void*Body(const void*name,void*entity,void*,bool){
 Ogre::Entity*e=static_cast<Ogre::Entity*>(entity);unsigned expectedRefs=e->getMesh().useCount()-(morph_pose_scope::current&&morph_pose_scope::current->sourceLive?1:0);held=e->getMesh()->clone(*static_cast<const std::string*>(name),"General");
 borrowedSeen=held->getPoseCount()>0&&held->getPose(0)==e->getMesh()->getPose(0);
 pinIdentityValid=!morph_pose_scope::current->sourceLive||(memcmp(&morph_pose_scope::current->Source(),&e->getMesh(),sizeof(Ogre::MeshPtr))==0&&morph_pose_scope::current->targetLive&&memcmp(&morph_pose_scope::current->Target(),&held,sizeof(Ogre::MeshPtr))==0);
 Blend(held.get());
 if(mode==1)return entity; // abnormal native path leaves poses for finally
 if(mode==2)throw std::runtime_error("native body C++ fault");
 if(mode==3)RaiseException(0xe0421001,0,0,NULL);
 if(mode==4)fixes::g_fixesCfg.morphPoseReuseEnabled=false;
 if(mode==5){held->unload();return entity;}
 if(mode==7){fixes::g_fixesCfg.morphPoseReuseEnabled=false;morph_pose_scope::Create(NestedRemove,NULL,entity,NULL,false);fixes::g_fixesCfg.morphPoseReuseEnabled=true;return entity;}
 if(mode==8){Ogre::Entity*nested=activeScene->createEntity(held);std::string nestedName="nested-from-borrowed-target";morph_pose_scope::Create(NestedTarget,&nestedName,nested,NULL,false);activeScene->destroyEntity(nested);nestedHeld.setNull();}
 if(mode==6)morph_pose_scope::Poses(held.get()).push_back(DeepClone(e->getMesh()->getPose(0)));
 unsigned targetRefs=held.useCount()-(morph_pose_scope::current&&morph_pose_scope::current->targetLive?1:0);
 BoundaryRemove(held.get());
 boundaryRefsRestored=e->getMesh().useCount()==expectedRefs&&held.useCount()==targetRefs;
 return entity;
}
static bool SehCall(const std::string*name,Ogre::Entity*e){bool caught=false;__try{morph_pose_scope::Create(Body,name,e,NULL,false);}__except(GetExceptionCode()==0xe0421001?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){caught=true;}return caught;}
static int Run(Ogre::SceneManager*scene,unsigned poses){
 Ogre::MeshPtr source=Model(0,poses);Ogre::Entity*entity=NULL;try{entity=scene->createEntity(source);}catch(const Ogre::Exception&e){printf("ENTITY %s\n",e.getFullDescription().c_str());return 1;}
 unsigned sourceRefs=source.useCount();std::vector<unsigned char>initial=Bytes(source.get());
 std::vector<Ogre::Pose::VertexOffsetMap>offsets;std::vector<Ogre::Pose::NormalsMap>normals;
 for(unsigned i=0;i<poses;++i){offsets.push_back(source->getPose(i)->getVertexOffsets());normals.push_back(source->getPose(i)->getNormals());}
 char text[64];sprintf_s(text,"scope-off-%u",++serial);std::string name=text;fixes::g_fixesCfg.morphPoseReuseEnabled=false;deepClones=0;mode=0;
 C(morph_pose_scope::Create(Body,&name,entity,NULL,false)==entity);C(deepClones==poses&&!borrowedSeen);std::vector<unsigned char>expected=Bytes(held.get());C(held->getPoseCount()==0);held.setNull();
 fixes::g_fixesCfg.morphPoseReuseEnabled=true;
 for(mode=0;mode<=6;++mode){sprintf_s(text,"scope-on-%u",++serial);name=text;deepClones=0;
  bool caught=false;if(mode==2){try{morph_pose_scope::Create(Body,&name,entity,NULL,false);}catch(const std::runtime_error&){caught=true;}C(caught);}
  else if(mode==3)C(SehCall(&name,entity));else C(morph_pose_scope::Create(Body,&name,entity,NULL,false)==entity);
  C(pinIdentityValid);C(!unsafeRemoval);C(deepClones==(mode==6?1u:0u)&&borrowedSeen);C(held->getPoseCount()==0);if(mode!=5)C(Bytes(held.get())==expected);if(mode==0||mode==4||mode==6){C(boundaryRefsRestored);C(keptAtRemove==(mode==6?1u:0u));}C(source.useCount()==sourceRefs);
  C(morph_pose_scope::current==NULL&&morph_pose_scope::cloning==NULL&&morph_pose_scope::tracked==NULL&&morph_pose_scope::depth==0);
  C(Bytes(source.get())==initial);
  for(unsigned i=0;i<poses;++i){C(source->getPose(i)->getVertexOffsets()==offsets[i]);C(source->getPose(i)->getNormals()==normals[i]);}
  held.setNull();fixes::g_fixesCfg.morphPoseReuseEnabled=true;
 }
 scene->destroyEntity(entity);return 0;
}
static bool cloneExceptionRestored=false;
static void*CatchCloneBody(const void*name,void*entity,void*,bool){
 Ogre::Entity*e=static_cast<Ogre::Entity*>(entity);bool caught=false;
 try{held=e->getMesh()->clone(*static_cast<const std::string*>(name),"General");}catch(const Ogre::Exception&){caught=true;}
 cloneExceptionRestored=caught&&morph_pose_scope::cloning==NULL&&morph_pose_scope::current&&morph_pose_scope::current->count==3;return entity;
}
static unsigned depthCalls=0,maxDepth=0;static bool depthMasked=false;
static void*DepthBody(const void*name,void*entity,void*,bool){
 ++depthCalls;if(morph_pose_scope::depth>maxDepth)maxDepth=morph_pose_scope::depth;
 if(depthCalls<12)morph_pose_scope::Create(DepthBody,name,entity,NULL,false);
 else {depthMasked=morph_pose_scope::current==NULL;Ogre::Entity*e=static_cast<Ogre::Entity*>(entity);held=e->getMesh()->clone(*static_cast<const std::string*>(name),"General");held->removeAllPoses();}
 return entity;
}
static int EdgeCases(Ogre::SceneManager*scene){
 Ogre::MeshPtr source=Model(0,8);Ogre::Entity*entity=scene->createEntity(source);unsigned refs=source.useCount();std::string name;
 for(unsigned gate=0;gate<8;++gate){char text[64];sprintf_s(text,"gate-%u",++serial);name=text;
  if(gate==0)owner=false;if(gate==1)morph_pose_scope::admitted=false;if(gate==2)morph_pose_scope::SetCloneHooksReady(false);if(gate==3)morph_pose_scope::SetCreateHookReady(false);
  if(gate==4)fixes::g_fixesCfg.morphPoseReuseEnabled=false;if(gate==5)wrongCloneCaller=true;if(gate==6)wrongCaptureCaller=true;if(gate==7)wrongCaptureResult=true;
  mode=0;deepClones=0;C(morph_pose_scope::Create(Body,&name,entity,NULL,false)==entity);C(deepClones==8&&!borrowedSeen&&held->getPoseCount()==0);C(source.useCount()==refs);
  owner=true;morph_pose_scope::admitted=true;morph_pose_scope::SetCloneHooksReady(true);morph_pose_scope::SetCreateHookReady(true);fixes::g_fixesCfg.morphPoseReuseEnabled=true;wrongCloneCaller=wrongCaptureCaller=wrongCaptureResult=false;held.setNull();
 }
 rejectAfter=3;mode=0;deepClones=0;name="mixed-prefix";C(morph_pose_scope::Create(Body,&name,entity,NULL,false)==entity);C(deepClones==5&&borrowedSeen&&keptAtRemove==5&&boundaryRefsRestored);held.setNull();
 for(mode=2;mode<=3;++mode){name=mode==2?"mixed-cxx":"mixed-seh";deepClones=0;bool caught=false;
  if(mode==2){try{morph_pose_scope::Create(Body,&name,entity,NULL,false);}catch(const std::runtime_error&){caught=true;}C(caught);}else C(SehCall(&name,entity));
  C(deepClones==5&&held->getPoseCount()==5&&source.useCount()==refs);for(size_t i=0;i<held->getPoseCount();++i)C(held->getPose((unsigned short)i)!=source->getPose((unsigned short)(i+3)));
  held->removeAllPoses();held.setNull();
 }
 name="caught-native-clone-fault";faultSource=source.get();deepFault=true;cloneExceptionRestored=false;
 C(morph_pose_scope::Create(CatchCloneBody,&name,entity,NULL,false)==entity);deepFault=false;faultSource=NULL;C(cloneExceptionRestored&&source.useCount()==refs);
 mode=0;name="partial-native-clone-fault";faultSource=source.get();deepFault=true;bool caught=false;
 try{morph_pose_scope::Create(Body,&name,entity,NULL,false);}catch(const Ogre::Exception&){caught=true;}
 deepFault=false;faultSource=NULL;rejectAfter=0;C(caught&&source.useCount()==refs&&morph_pose_scope::tracked==NULL&&morph_pose_scope::cloning==NULL);
 Ogre::MeshPtr partial=Ogre::MeshManager::getSingleton().getByName(name,"General");C(!partial.isNull()&&partial->getPoseCount()==0);partial.setNull();held.setNull();
 for(mode=7;mode<=8;++mode){name=mode==7?"nested-off-parent-remove":"nested-target-fallback";deepClones=0;nestedSafe=false;
  C(morph_pose_scope::Create(Body,&name,entity,NULL,false)==entity);C(!unsafeRemoval);C(nestedSafe);C(deepClones==(mode==8?8u:0u)&&held->getPoseCount()==0&&source.useCount()==refs);held.setNull();
 }
 mode=0;name="nested-direct-mask";deepClones=0;injectNested=true;nestedMaskOk=false;C(morph_pose_scope::Create(Body,&name,entity,NULL,false)==entity);C(nestedMaskOk&&deepClones==8&&borrowedSeen&&source.useCount()==refs);held.setNull();
 name="depth-bound";depthCalls=maxDepth=0;depthMasked=false;deepClones=0;C(morph_pose_scope::Create(DepthBody,&name,entity,NULL,false)==entity);C(depthCalls==12&&maxDepth==8&&depthMasked&&deepClones==8&&source.useCount()==refs);C(morph_pose_scope::current==NULL&&morph_pose_scope::tracked==NULL&&morph_pose_scope::depth==0);held.setNull();
 Ogre::MeshPtr outside=source->clone("outside-private-scope","General");C(outside->getPose(0)!=source->getPose(0));outside->removeAllPoses();outside.setNull();
 scene->destroyEntity(entity);
 for(unsigned bound=0;bound<2;++bound){Ogre::MeshPtr unusual=Model(0,bound?513:0);Ogre::Entity*e=scene->createEntity(unusual);unsigned before=unusual.useCount();name=bound?"above-pose-bound":"empty-pose-source";mode=0;deepClones=0;
  C(morph_pose_scope::Create(Body,&name,e,NULL,false)==e);C(!borrowedSeen&&deepClones==(bound?513u:0u)&&held->getPoseCount()==0&&unusual.useCount()==before);held.setNull();scene->destroyEntity(e);
 }
 return 0;
}
struct Cleanup{~Cleanup(){if(activeScene){Ogre::Root::getSingleton().destroySceneManager(activeScene);activeScene=NULL;}held.setNull();nestedHeld.setNull();Ogre::MeshManager::getSingleton().removeAll();}};
static LONG WINAPI Crash(EXCEPTION_POINTERS*p){printf("CRASH %08lx RIP%p Ogre%p\n",p->ExceptionRecord->ExceptionCode,(void*)p->ContextRecord->Rip,GetModuleHandleA("OgreMain_x64.dll"));return EXCEPTION_EXECUTE_HANDLER;}
int main(){SetUnhandledExceptionFilter(Crash);setvbuf(stdout,NULL,_IONBF,0);C(InitSdk()==0);Ogre::Root root("","","");Ogre::DefaultHardwareBufferManager hardware;Cleanup cleanup;
 HMODULE ogre=GetModuleHandleA("OgreMain_x64.dll");C(DiskHash(ogre,"7d3888d21398647519f10a89b1f300d8606f761ad159e6021a797b1b9b3df5f6"));
 C(AdmissionCases(ogre)==0);
 C(sdkAdd((char*)ogre+0x1a3ee0,(void*)&CloneBridge,(void**)&nativeClone)==0);
 C(sdkAdd((char*)ogre+0x1be970,(void*)&CreateBridge,(void**)&nativeCreate)==0);
 morph_pose_scope::Install(NULL,NULL);C(morph_pose_scope::admitted&&hookCalls==2&&morph_pose_scope::originalPose&&morph_pose_scope::originalRemove);
 nativePose=morph_pose_scope::originalPose;morph_pose_scope::originalPose=DeepClone;actualRemove=morph_pose_scope::originalRemove;morph_pose_scope::originalRemove=RemoveSpy;
 Ogre::MeshPtr calibration=Model(0,0);BoundaryRemove(calibration.get());morph_pose_scope::gameBase=boundaryReturn-0x71ce2;boundaryFunction=morph_pose_scope::Remove;
 Ogre::MaterialManager::getSingleton().initialise();Ogre::SceneManager*scene=root.createSceneManager(Ogre::ST_GENERIC,1,Ogre::INSTANCING_CULLING_SINGLETHREAD);
 activeScene=scene;C(Run(scene,8)==0);weightMode=1;C(Run(scene,128)==0);weightMode=0;C(Run(scene,512)==0);separateNormals=true;C(Run(scene,8)==0);separateNormals=false;
 C(EdgeCases(scene)==0);root.destroySceneManager(scene);activeScene=NULL;VirtualFree(fakeGame,0,MEM_RELEASE);fakeGame=NULL;std::printf("PASS %u actual native pose-scope checks\n",checks);return 0;
}
