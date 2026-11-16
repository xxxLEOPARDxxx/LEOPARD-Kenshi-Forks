// PRIVATE offline fixture. Real Ogre DLLs, no game executable or process APIs.
#define KEO_MODEL_CLONE_RESERVE_TEST
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>
#include <ogre/OgreRoot.h>
#include <ogre/OgreMeshManager.h>
#include <ogre/OgreMesh.h>
#include <ogre/OgrePose.h>
#include <ogre/OgreException.h>
#include <ogre/OgreSubMesh.h>
#include <ogre/OgreDefaultHardwareBufferManager.h>
#include <ogre/OgreVertexIndexData.h>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstring>
static bool owner=true;
bool IsMainThread(){return owner;}
namespace fixes {struct Config{bool modelCloneReserveEnabled;};Config g_fixesCfg={true};}
static unsigned checks=0,reserves=0,errors=0;
static bool reserveFault=false,throwLogger=false;static unsigned failReserveAt=0;
void LogError(const std::string&){++errors;if(throwLogger)throw std::bad_alloc();}
#define KEO_DETAIL(x) ((void)0)
typedef int(*SdkAdd)(void*,void*,void**);
static SdkAdd sdkAdd=NULL;
static bool fakeHooks=false;static unsigned hookCalls=0,failHook=0,nullHook=0;
static HMODULE moduleOverride=NULL;static bool overrideModule=false;static unsigned wrongSymbol=0;
static FARPROC Symbol(HMODULE module,const char*symbol){
 if(overrideModule&&module==moduleOverride){bool clone=strstr(symbol,"clone@Mesh@")!=NULL;return (FARPROC)((char*)module+(clone?0x1a3ee0:0x1be970)+(wrongSymbol==(clone?1u:2u)?1:0));}
 return GetProcAddress(module,symbol);
}
#define KEO_MODEL_CLONE_MODULE() (overrideModule?moduleOverride:GetModuleHandleA("OgreMain_x64.dll"))
#define KEO_MODEL_CLONE_SYMBOL(module,symbol) Symbol(module,symbol)
static void* FakeClone(void*,void*,const void*,const void*);
static void* FakeCreate(void*,void*,const void*,const void*,void*);
namespace KenshiLib {enum Status{SUCCESS,FAILURE};Status AddHook(void*t,void*h,void**out){
 ++hookCalls;if(fakeHooks){*out=hookCalls==nullHook?NULL:hookCalls%2?(void*)&FakeClone:(void*)&FakeCreate;return hookCalls==failHook?FAILURE:SUCCESS;}
 return (Status)sdkAdd(t,h,out);
}}
bool ReadModuleImageId(HMODULE m,DWORD*stamp,DWORD*size){
 const IMAGE_DOS_HEADER*d=(const IMAGE_DOS_HEADER*)m;if(d->e_magic!=IMAGE_DOS_SIGNATURE||d->e_lfanew<0||d->e_lfanew>1024*1024)return false;
 const IMAGE_NT_HEADERS64*n=(const IMAGE_NT_HEADERS64*)((const char*)m+d->e_lfanew);
 if(n->Signature!=IMAGE_NT_SIGNATURE||n->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64)return false;
 *stamp=n->FileHeader.TimeDateStamp;*size=n->OptionalHeader.SizeOfImage;return true;
}
template<class T>static void ReserveOperation(T&v,size_t n){++reserves;if(reserveFault||reserves==failReserveAt)throw std::bad_alloc();v.reserve(n);}
#define KEO_MODEL_CLONE_RESERVE_OPERATION(v,n) ReserveOperation(v,n)
#ifndef KEO_MODEL_CLONE_SOURCE
#define KEO_MODEL_CLONE_SOURCE "../src/fixes/world/model_clone_reserve.cpp"
#endif
#include KEO_MODEL_CLONE_SOURCE
struct Lists:Ogre::Mesh {
 using Ogre::Mesh::mSubMeshList;
 static SubMeshList&Subs(Ogre::Mesh*p){SubMeshList Ogre::Mesh::*m=static_cast<SubMeshList Ogre::Mesh::*>(&Lists::mSubMeshList);return p->*m;}
};
static Ogre::PoseList&Poses(Ogre::Mesh*m){return const_cast<Ogre::PoseList&>(m->getPoseList());}
#define C(x) do{++checks;if(!(x)){std::printf("CHECK line%d %s\n",__LINE__,#x);return 1;}}while(0)
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
static bool tracking=false,pendingPose=false;
static Ogre::Mesh*destination=NULL;
static std::string cloneName;
static unsigned subGrowth=0,poseGrowth=0;
static size_t pendingCapacity=0;
typedef Ogre::SubMesh*(*SubCreate)(Ogre::Mesh*);
typedef Ogre::Pose*(*PoseClone)(const Ogre::Pose*);
static SubCreate nativeSub=NULL;static PoseClone nativePose=NULL;
static Ogre::SubMesh*SubProbe(Ogre::Mesh*m){
 size_t before=Lists::Subs(m).capacity();Ogre::SubMesh*s=nativeSub(m);
 if(tracking){destination=m;subGrowth+=before!=Lists::Subs(m).capacity();}return s;
}
static void CompletePose(){if(pendingPose){poseGrowth+=Poses(destination).capacity()!=pendingCapacity;pendingPose=false;}}
static Ogre::Pose*PoseProbe(const Ogre::Pose*p){
 if(tracking){
  if(!destination){Ogre::MeshPtr m=Ogre::MeshManager::getSingleton().getByName(cloneName,"General");destination=m.get();if(!destination)throw std::runtime_error("fixture destination not registered");}
  CompletePose();pendingCapacity=Poses(destination).capacity();pendingPose=true;
 }
 return nativePose(p);
}
struct Loader:Ogre::ManualResourceLoader{void loadResource(Ogre::Resource*){}};
static Loader loader;static unsigned serial=0;
static Ogre::VertexData*Vertices(unsigned seed){
 Ogre::VertexData*v=OGRE_NEW Ogre::VertexData;v->vertexStart=0;v->vertexCount=4;
 v->vertexDeclaration->addElement(0,0,Ogre::VET_FLOAT3,Ogre::VES_POSITION);
 Ogre::HardwareVertexBufferSharedPtr b=Ogre::HardwareBufferManager::getSingleton().createVertexBuffer(12,4,Ogre::HardwareBuffer::HBU_STATIC_WRITE_ONLY,false);
 float data[12];for(unsigned i=0;i<12;++i)data[i]=float(seed*13+i)/7.0f;b->writeData(0,sizeof(data),data,true);v->vertexBufferBinding->setBinding(0,b);return v;
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
static int CompareVertices(const Ogre::VertexData*a,const Ogre::VertexData*b){
 C((a==NULL)==(b==NULL));if(!a)return 0;C(a!=b);C(a->vertexStart==b->vertexStart&&a->vertexCount==b->vertexCount);
 C(a->vertexDeclaration->getElements()==b->vertexDeclaration->getElements());
 const Ogre::VertexBufferBinding::VertexBufferBindingMap&x=a->vertexBufferBinding->getBindings(),&y=b->vertexBufferBinding->getBindings();C(x.size()==y.size());
 Ogre::VertexBufferBinding::VertexBufferBindingMap::const_iterator i=x.begin(),j=y.begin();
 for(;i!=x.end();++i,++j){C(i->first==j->first);C(i->second.get()!=j->second.get());C(i->second->getSizeInBytes()==j->second->getSizeInBytes());
  std::vector<unsigned char>u(i->second->getSizeInBytes()),v(j->second->getSizeInBytes());i->second->readData(0,u.size(),&u[0]);j->second->readData(0,v.size(),&v[0]);C(u==v);}
 return 0;
}
static int Compare(const Ogre::MeshPtr&a,const Ogre::MeshPtr&b){
 C(a.get()!=b.get());C(a->isLoaded()&&b->isLoaded());C(a->getNumSubMeshes()==b->getNumSubMeshes());C(a->getPoseCount()==b->getPoseCount());
 const Ogre::Mesh::SubMeshNameMap&na=a->getSubMeshNameMap(),&nb=b->getSubMeshNameMap();C(na.size()==nb.size());
 for(Ogre::Mesh::SubMeshNameMap::const_iterator k=na.begin();k!=na.end();++k){Ogre::Mesh::SubMeshNameMap::const_iterator v=nb.find(k->first);C(v!=nb.end()&&v->second==k->second);}
 C(a->getBounds()==b->getBounds());C(a->getBoundingSphereRadius()==b->getBoundingSphereRadius());C(CompareVertices(a->sharedVertexData,b->sharedVertexData)==0);
 for(unsigned i=0;i<a->getNumSubMeshes();++i){Ogre::SubMesh*x=a->getSubMesh((unsigned short)i),*y=b->getSubMesh((unsigned short)i);
  C(x!=y&&x->parent==a.get()&&y->parent==b.get());C(x->useSharedVertices==y->useSharedVertices);C(x->operationType==y->operationType);C(x->getMaterialName()==y->getMaterialName());C(x->extremityPoints==y->extremityPoints);C(CompareVertices(x->vertexData,y->vertexData)==0);
  C(x->indexData!=y->indexData);C(x->indexData->indexStart==y->indexData->indexStart&&x->indexData->indexCount==y->indexData->indexCount);C(x->indexData->indexBuffer.get()!=y->indexData->indexBuffer.get());
  size_t n=x->indexData->indexBuffer->getSizeInBytes();C(n==y->indexData->indexBuffer->getSizeInBytes());std::vector<unsigned char>u(n),v(n);x->indexData->indexBuffer->readData(0,n,&u[0]);y->indexData->indexBuffer->readData(0,n,&v[0]);C(u==v);
 }
 for(size_t i=0;i<a->getPoseCount();++i){const Ogre::Pose*x=a->getPose((unsigned short)i),*y=b->getPose((unsigned short)i);C(x!=y);C(x->getName()==y->getName());C(x->getTarget()==y->getTarget());C(x->getVertexOffsets()==y->getVertexOffsets());C(x->getNormals()==y->getNormals());C(x->getIncludesNormals()==y->getIncludesNormals());}
 return 0;
}
struct Counts{unsigned sub,pose,reserve;};
static Ogre::MeshPtr MeasuredClone(const Ogre::MeshPtr&m,bool enabled,Counts*c){
 char name[64];sprintf_s(name,"fixture-clone-%u",++serial);cloneName=name;destination=NULL;pendingPose=false;subGrowth=poseGrowth=reserves=0;fixes::g_fixesCfg.modelCloneReserveEnabled=enabled;tracking=true;
 Ogre::MeshPtr result=m->clone(name,"General");CompletePose();tracking=false;c->sub=subGrowth;c->pose=poseGrowth;c->reserve=reserves;return result;
}
static int Case(unsigned sub,unsigned pose){
 Ogre::MeshPtr m=Model(sub,pose);Counts a,b;Ogre::MeshPtr baseline=MeasuredClone(m,false,&a),optimized=MeasuredClone(m,true,&b);
 std::printf("native clone %u sub/%u pose: OFF growth%u/%u ON reserve%u growth%u/%u\n",sub,pose,a.sub,a.pose,b.reserve,b.sub,b.pose);
 C(Compare(m,baseline)==0);C(Compare(m,optimized)==0);C(Compare(baseline,optimized)==0);C(a.reserve==0);
 C(b.reserve==unsigned(sub>0&&sub<=64)+unsigned(pose>0&&pose<=512));C(sub>64||b.sub==0);C(pose>512||b.pose==0);
 if(sub>1&&sub<=64)C(a.sub>1);if(pose>1&&pose<=512)C(a.pose>1);return 0;
}
static unsigned cloneCalls=0,createCalls=0;static bool throwNative=false;
static void*seenSource=NULL;static void*args[5]={};static void*returnOverride=NULL;
static unsigned nested=0;static bool nestedRestored=false,nestedSuppressed=false;
static void* FakeClone(void*self,void*out,const void*name,const void*group){
 ++cloneCalls;seenSource=model_clone_reserve::source;args[0]=self;args[1]=out;args[2]=(void*)name;args[3]=(void*)group;
 if(throwNative)throw std::runtime_error("native clone");
 if(nested%2==1){unsigned kind=nested;nested++;if(kind==1)owner=false;if(kind==3)fixes::g_fixesCfg.modelCloneReserveEnabled=false;if(kind==5)model_clone_reserve::admitted=false;
  model_clone_reserve::Clone((void*)2,out,name,group);owner=true;fixes::g_fixesCfg.modelCloneReserveEnabled=true;model_clone_reserve::admitted=true;
  nestedSuppressed=seenSource==(kind==7?(void*)2:NULL);nestedRestored=model_clone_reserve::source==self;}
 return returnOverride?returnOverride:out;
}
static void* FakeCreate(void*self,void*out,const void*name,const void*group,void*load){++createCalls;args[0]=self;args[1]=out;args[2]=(void*)name;args[3]=(void*)group;args[4]=load;if(throwNative)throw std::runtime_error("native create");return returnOverride?returnOverride:out;}
static int SpyCases(){
 model_clone_reserve::originalClone=FakeClone;model_clone_reserve::originalCreate=FakeCreate;model_clone_reserve::admitted=true;
 void*self=(void*)0x101,*out=(void*)0x202,*name=(void*)0x303,*group=(void*)0x404,*load=(void*)0x505;
 model_clone_reserve::source=(void*)0x999;cloneCalls=createCalls=reserves=0;
 C(model_clone_reserve::Clone(self,out,name,group)==out);C(cloneCalls==1&&seenSource==self);C(args[0]==self&&args[1]==out&&args[2]==name&&args[3]==group);C(model_clone_reserve::source==(void*)0x999);
 for(unsigned kind=0;kind<3;++kind){if(kind==0)owner=false;if(kind==1)model_clone_reserve::admitted=false;if(kind==2)fixes::g_fixesCfg.modelCloneReserveEnabled=false;
  C(model_clone_reserve::Clone(self,out,name,group)==out);C(seenSource==NULL&&model_clone_reserve::source==(void*)0x999);owner=true;model_clone_reserve::admitted=true;fixes::g_fixesCfg.modelCloneReserveEnabled=true;}
 for(unsigned kind=1;kind<=7;kind+=2){nested=kind;C(model_clone_reserve::Clone(self,out,name,group)==out);C(nestedSuppressed&&nestedRestored&&model_clone_reserve::source==(void*)0x999);}nested=0;
 returnOverride=(void*)0x777;C(model_clone_reserve::Clone(self,out,name,group)==returnOverride);C(model_clone_reserve::CreateManual(self,out,name,group,load)==returnOverride);C(createCalls==1&&args[0]==self&&args[1]==out&&args[2]==name&&args[3]==group&&args[4]==load);C(reserves==0);returnOverride=NULL;
 throwNative=true;unsigned before=cloneCalls;try{model_clone_reserve::Clone(self,out,name,group);C(false);}catch(const std::runtime_error&){}C(cloneCalls==before+1&&model_clone_reserve::source==(void*)0x999);
 before=createCalls;try{model_clone_reserve::CreateManual(self,out,name,group,load);C(false);}catch(const std::runtime_error&){}C(createCalls==before+1);throwNative=false;model_clone_reserve::source=NULL;return 0;
}
static int AdmissionCases(HMODULE ogre){
 C(model_clone_reserve::Admit(ogre));C(!model_clone_reserve::Admit(NULL));C(!model_clone_reserve::Admit((HMODULE)1));
 char*fake=(char*)VirtualAlloc(NULL,0x9c9000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);C(fake!=NULL);memcpy(fake,ogre,4096);
 for(unsigned i=0;i<sizeof(model_clone_reserve::proofs)/sizeof(model_clone_reserve::proofs[0]);++i){const model_clone_reserve::Proof&p=model_clone_reserve::proofs[i];memcpy(fake+p.rva,p.bytes,p.size);}
 overrideModule=true;moduleOverride=(HMODULE)fake;C(model_clone_reserve::Admit(moduleOverride));
 for(unsigned i=1;i<=2;++i){wrongSymbol=i;C(!model_clone_reserve::Admit(moduleOverride));}wrongSymbol=0;
 for(unsigned i=0;i<sizeof(model_clone_reserve::proofs)/sizeof(model_clone_reserve::proofs[0]);++i){uintptr_t r=model_clone_reserve::proofs[i].rva;fake[r]^=1;C(!model_clone_reserve::Admit(moduleOverride));fake[r]^=1;}
 IMAGE_NT_HEADERS64*nt=(IMAGE_NT_HEADERS64*)(fake+((IMAGE_DOS_HEADER*)fake)->e_lfanew);nt->FileHeader.TimeDateStamp^=1;C(!model_clone_reserve::Admit(moduleOverride));nt->FileHeader.TimeDateStamp^=1;nt->OptionalHeader.SizeOfImage^=1;C(!model_clone_reserve::Admit(moduleOverride));nt->OptionalHeader.SizeOfImage^=1;
 fakeHooks=true;
 for(unsigned fault=0;fault<5;++fault){model_clone_reserve::tried=model_clone_reserve::admitted=false;model_clone_reserve::originalClone=NULL;model_clone_reserve::originalCreate=NULL;hookCalls=0;failHook=fault<3?fault:0;nullHook=fault>=3?fault-2:0;
  model_clone_reserve::Install();C(model_clone_reserve::tried);C(model_clone_reserve::admitted==(fault==0));C(hookCalls==((fault==1||fault==3)?1u:2u));
  if(fault==1||fault==2)C(model_clone_reserve::originalClone!=NULL);if(fault==2)C(model_clone_reserve::originalCreate!=NULL);unsigned before=hookCalls;model_clone_reserve::Install();C(hookCalls==before);}
 fakeHooks=false;failHook=nullHook=0;overrideModule=false;moduleOverride=NULL;VirtualFree(fake,0,MEM_RELEASE);model_clone_reserve::tried=model_clone_reserve::admitted=false;model_clone_reserve::originalClone=NULL;model_clone_reserve::originalCreate=NULL;hookCalls=0;return 0;
}
static int PrepareCases(){
 Ogre::MeshPtr src=Model(8,128),dst=Model(0,0),nil;reserves=0;model_clone_reserve::Prepare(NULL,&dst);model_clone_reserve::Prepare(src.get(),NULL);model_clone_reserve::Prepare(src.get(),&nil);model_clone_reserve::Prepare(src.get(),&src);C(reserves==0);
 Ogre::MeshPtr unloaded=Model(8,8,false);model_clone_reserve::Prepare(unloaded.get(),&dst);C(reserves==0);
 model_clone_reserve::Prepare(src.get(),&dst);C(reserves==2&&Lists::Subs(dst.get()).empty()&&Poses(dst.get()).empty());C(Lists::Subs(dst.get()).capacity()>=8&&Poses(dst.get()).capacity()>=128);
 reserves=0;model_clone_reserve::Prepare(src.get(),&dst);C(reserves==0);
 Ogre::MeshPtr nonempty=Model(1,1);model_clone_reserve::Prepare(src.get(),&nonempty);C(reserves==0&&nonempty->getNumSubMeshes()==1&&nonempty->getPoseCount()==1);
 Ogre::MeshPtr onlySub=Model(1,0);model_clone_reserve::Prepare(src.get(),&onlySub);C(reserves==1&&Lists::Subs(onlySub.get()).size()==1&&Poses(onlySub.get()).empty()&&Poses(onlySub.get()).capacity()>=128);
 reserves=0;Ogre::MeshPtr onlyPose=Model(0,1);model_clone_reserve::Prepare(src.get(),&onlyPose);C(reserves==1&&Lists::Subs(onlyPose.get()).empty()&&Lists::Subs(onlyPose.get()).capacity()>=8&&Poses(onlyPose.get()).size()==1);reserves=0;
 Ogre::MeshPtr huge=Model(65,513),empty=Model(0,0);model_clone_reserve::Prepare(huge.get(),&empty);C(reserves==0&&Lists::Subs(empty.get()).capacity()==0&&Poses(empty.get()).capacity()==0);
 for(unsigned fault=1;fault<=3;++fault){Counts c;errors=0;failReserveAt=fault==2?2:0;reserveFault=fault!=2;throwLogger=fault==3;
  Ogre::MeshPtr m;bool escaped=false;try{m=MeasuredClone(src,true,&c);}catch(...){escaped=true;tracking=pendingPose=false;}
  reserveFault=throwLogger=false;failReserveAt=0;C(!escaped);C(errors==1);C(c.reserve==(fault==2?2u:1u));C(c.sub==(fault==2?0u:6u));C(c.pose==13);C(Compare(src,m)==0);}
 return 0;
}
static model_clone_reserve::CreateFn realCreate=NULL;
static void*AlternateNativeCreate(void*self,void*out,const void*name,const void*group,void*load){realCreate(self,out,name,group,load);return (char*)out+16;}
static Ogre::Mesh*exceptionSource=NULL;
static void*ThrowNativeCreate(void*,void*,const void*,const void*,void*){exceptionSource->getSubMesh(65535);return NULL;}
static int NativeGateCases(){
 Ogre::MeshPtr src=Model(8,128);Counts c;
 for(unsigned kind=0;kind<3;++kind){uintptr_t old=model_clone_reserve::moduleBase;if(kind==0)owner=false;if(kind==1)model_clone_reserve::admitted=false;if(kind==2)++model_clone_reserve::moduleBase;
  Ogre::MeshPtr m=MeasuredClone(src,true,&c);owner=true;model_clone_reserve::admitted=true;model_clone_reserve::moduleBase=old;C(c.reserve==0&&c.sub==6&&c.pose==13);C(Compare(src,m)==0);}
 // A real unrelated createManual while a source lease exists is never prepared.
 model_clone_reserve::source=src.get();reserves=0;char name[64];sprintf_s(name,"direct-manual-%u",++serial);
 Ogre::MeshPtr direct=Ogre::MeshManager::getSingleton().createManual(name,"General",&loader);C(reserves==0&&Lists::Subs(direct.get()).capacity()==0&&Poses(direct.get()).capacity()==0);model_clone_reserve::source=NULL;
 // Native clone ignores createManual's RAX and consumes its hidden out storage.
 // An intentionally alternate raw return tests the wrapper's success-result gate.
 realCreate=model_clone_reserve::originalCreate;model_clone_reserve::originalCreate=AlternateNativeCreate;
 Ogre::MeshPtr alternate=MeasuredClone(src,true,&c);model_clone_reserve::originalCreate=realCreate;C(c.reserve==0&&c.sub==6&&c.pose==13);C(Compare(src,alternate)==0);
 // Inject a genuine native Ogre bounds exception at the manager boundary;
 // it crosses the actual Mesh::clone stack and both production wrappers.
 exceptionSource=src.get();model_clone_reserve::originalCreate=ThrowNativeCreate;
 model_clone_reserve::source=(void*)0x777;bool caught=false;try{src->clone("native-exception-fixture","General");}catch(const Ogre::Exception&){caught=true;}
 model_clone_reserve::originalCreate=realCreate;exceptionSource=NULL;
 C(caught&&model_clone_reserve::source==(void*)0x777);model_clone_reserve::source=NULL;return 0;
}
static LONG WINAPI Crash(EXCEPTION_POINTERS*p){std::printf("FIXTURE CRASH code%08lx RIP%p Ogre%p SDK%p\n",p->ExceptionRecord->ExceptionCode,(void*)p->ContextRecord->Rip,GetModuleHandleA("OgreMain_x64.dll"),GetModuleHandleA("KenshiLib.dll"));std::fflush(stdout);return EXCEPTION_EXECUTE_HANDLER;}
struct Cleanup{~Cleanup(){Ogre::MeshManager::getSingleton().removeAll();}};
int main(){
 SetUnhandledExceptionFilter(Crash);setvbuf(stdout,NULL,_IONBF,0);
 C(InitSdk()==0);Ogre::Root root("","","");Ogre::DefaultHardwareBufferManager hardware;Cleanup cleanup;HMODULE ogre=GetModuleHandleA("OgreMain_x64.dll");C(ogre!=NULL);
 C(DiskHash(ogre,"7d3888d21398647519f10a89b1f300d8606f761ad159e6021a797b1b9b3df5f6"));
 C(SpyCases()==0);C(AdmissionCases(ogre)==0);
 model_clone_reserve::Install();C(model_clone_reserve::admitted&&hookCalls==2&&model_clone_reserve::originalClone&&model_clone_reserve::originalCreate);
 C(sdkAdd((char*)ogre+0x1a2ff0,(void*)&SubProbe,(void**)&nativeSub)==0);C(sdkAdd((char*)ogre+0x229bf0,(void*)&PoseProbe,(void**)&nativePose)==0);
 model_clone_reserve::moduleBase=(uintptr_t)ogre;model_clone_reserve::admitted=true;
 C(PrepareCases()==0);C(Case(8,128)==0);C(Case(0,512)==0);C(Case(64,8)==0);C(Case(1,1)==0);C(Case(0,0)==0);C(Case(65,513)==0);C(Case(65,8)==0);C(Case(8,513)==0);C(NativeGateCases()==0);
 std::printf("PASS %u actual native model-clone checks\n",checks);return 0;
}
