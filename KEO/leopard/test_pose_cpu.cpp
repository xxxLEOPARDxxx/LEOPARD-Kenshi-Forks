// Oracle is the actual installed Ogre arithmetic in this fixture process only.
#define main ExistingMorphFixtureMain
#include "test_morph_pose_scope.cpp"
#undef main
#include "fixes/world/pose_cpu_kernel.h"
#include <xmmintrin.h>
#include <ogre/OgreMeshSerializer.h>
#include <ogre/OgreDataStream.h>
#include <fstream>
#include "fixes/world/pose_cpu_pool.h"
#define KEO_POSE_CPU_TEST
#include "pose_cpu_spike/fixes/world/pose_cpu_blend.cpp"
#ifdef KEO_POSE_CPU_BASELINE
namespace pose_cpu_kernel{void Run(const Work&,size_t,size_t){}}
namespace pose_cpu_pool{bool Execute(pose_cpu_kernel::Work*){return false;}bool Prepare(){return false;}void SignalStop(){}void Stop(){}}
#else
#include "pose_cpu_spike/fixes/world/pose_cpu_kernel.cpp"
#include "pose_cpu_spike/fixes/world/pose_cpu_pool.cpp"
#endif
static int KernelCases(){
 using namespace pose_cpu_kernel;
 Map positions,normals;for(size_t i=0;i<4;++i){positions[i]=Ogre::Vector3(float(i)*.03125f,-.75f,.2f);normals[i]=Ogre::Vector3(.001f,.3f,-.5f);}
 const unsigned saved=_mm_getcsr();const float weights[]={.25f,-.35f,1.0f,3.1415926f};
 for(unsigned rounding=0;rounding<4;++rounding)for(unsigned denorm=0;denorm<2;++denorm)for(unsigned mode=0;mode<4;++mode){
  const unsigned control=0x1f80|(rounding<<13)|(denorm?0x8040:0);_mm_setcsr(control);
  Ogre::VertexData*v=Vertices(5);const Ogre::HardwareVertexBufferSharedPtr&buffer=v->vertexBufferBinding->getBuffer(0);std::vector<unsigned char>start(buffer->getSizeInBytes()),expected(start.size()),actual(start.size());buffer->readData(0,start.size(),&start[0]);
  // Include IEEE payloads/subnormals in the target; arithmetic is scalar SSE.
  if(mode==3){unsigned bits[]={0x00000001,0x80000000,0x7fc12345};memcpy(&start[0],bits,sizeof(bits));buffer->writeData(0,start.size(),&start[0],true);}
  _mm_setcsr(control);Ogre::Mesh::softwareVertexPoseBlend(weights[mode],positions,normals,v);const unsigned nativeFlags=_mm_getcsr()&63;buffer->readData(0,expected.size(),&expected[0]);actual=start;
  Work work={weights[mode],&positions,&normals,&actual[0],buffer->getVertexSize(),4,12,true};_mm_setcsr(control);Run(work,0,2);Run(work,2,4);C(actual==expected);C((_mm_getcsr()&63)==nativeFlags);
  actual=start;work.bytes=&actual[0];_mm_setcsr(control);C(pose_cpu_pool::Execute(&work));C(actual==expected);C((_mm_getcsr()&63)==nativeFlags);delete v;
 }
 _mm_setcsr(saved);return 0;
}
static int Benchmark(){
 using namespace pose_cpu_kernel;
 LARGE_INTEGER frequency,a,b;QueryPerformanceFrequency(&frequency);
 const unsigned sizes[]={4096,16384,65536};
 for(unsigned mode=0;mode<3;++mode)for(unsigned size=0;size<3;++size){
  const size_t entries=sizes[size],vertices=mode?entries*4:entries;
  Map positions,normals;for(size_t i=0;i<entries;++i){const size_t key=mode==1?i*4:i;positions[key]=Ogre::Vector3(.03125f,-.75f,.2f);normals[key]=Ogre::Vector3(.001f,.3f,-.5f);}
  Ogre::VertexData v;v.vertexStart=0;v.vertexCount=vertices;v.vertexDeclaration->addElement(0,0,Ogre::VET_FLOAT3,Ogre::VES_POSITION);v.vertexDeclaration->addElement(0,12,Ogre::VET_FLOAT3,Ogre::VES_NORMAL);
  Ogre::HardwareVertexBufferSharedPtr buffer=Ogre::HardwareBufferManager::getSingleton().createVertexBuffer(24,vertices,Ogre::HardwareBuffer::HBU_STATIC,false);v.vertexBufferBinding->setBinding(0,buffer);
  std::vector<unsigned char>initial(vertices*24,0),expected(initial.size()),scratch(initial.size()),actual(initial.size());Work work={.25f,&positions,&normals,&scratch[0],24,vertices,12,true};double us[2];const unsigned iterations=80;
  for(unsigned path=0;path<2;++path){buffer->writeData(0,initial.size(),&initial[0],true);QueryPerformanceCounter(&a);for(unsigned i=0;i<iterations;++i){if(!path)Ogre::Mesh::softwareVertexPoseBlend(.25f,positions,normals,&v);else{buffer->readData(0,scratch.size(),&scratch[0]);C(pose_cpu_pool::Execute(&work));buffer->writeData(0,scratch.size(),&scratch[0],true);}}QueryPerformanceCounter(&b);us[path]=(b.QuadPart-a.QuadPart)*1e6/frequency.QuadPart/iterations;buffer->readData(0,initial.size(),path?&actual[0]:&expected[0]);}
  C(actual==expected);printf("BENCH mode=%u entries=%u vertices=%u native_us=%.2f parallel_two_copy_us=%.2f ratio=%.3f\n",mode,(unsigned)entries,(unsigned)vertices,us[0],us[1],us[1]/us[0]);
 }
 return 0;
}
class CpuShadow:public Ogre::HardwareVertexBuffer {
 std::vector<unsigned char>gpu;
protected:
 void*lockImpl(size_t off,size_t,Ogre::HardwareBuffer::LockOptions){return &gpu[0]+off;}
 void unlockImpl(){}
public:
 unsigned uploads;bool failUnlock;
 CpuShadow(size_t stride,size_t count):Ogre::HardwareVertexBuffer(NULL,stride,count,Ogre::HardwareBuffer::HBU_STATIC,false,true),gpu(stride*count,0),uploads(0),failUnlock(false){}
 void readData(size_t off,size_t len,void*out){memcpy(out,&gpu[0]+off,len);}
 void writeData(size_t off,size_t len,const void*in,bool){memcpy(&gpu[0]+off,in,len);mShadowBuffer->writeData(off,len,in,true);}
 void _updateFromShadow(){if(mShadowUpdated&&!mSuppressHardwareUpdate){++uploads;if(failUnlock)throw std::runtime_error("late upload");}Ogre::HardwareBuffer::_updateFromShadow();}
 const std::vector<unsigned char>&Bytes()const{return gpu;}
};
#ifndef KEO_POSE_CPU_BASELINE
static int BlendCases(){
 using namespace pose_cpu_kernel;
 const size_t count=16384;Map positions,normals;for(size_t i=0;i<count;++i){positions[i]=Ogre::Vector3(float(i)*.03125f,-.75f,.2f);normals[i]=Ogre::Vector3(.001f,.3f,-.5f);}
 Ogre::VertexData v;v.vertexStart=0;v.vertexCount=count;v.vertexDeclaration->addElement(0,0,Ogre::VET_FLOAT3,Ogre::VES_POSITION);v.vertexDeclaration->addElement(0,12,Ogre::VET_FLOAT3,Ogre::VES_NORMAL);
 CpuShadow*b=OGRE_NEW CpuShadow(32,count);Ogre::HardwareVertexBufferSharedPtr hold(b);v.vertexBufferBinding->setBinding(0,hold);std::vector<unsigned char>start(count*32,0),expected;
 pose_cpu_blend::Init();C(pose_cpu_blend::admitted);const unsigned saved=_mm_getcsr();
 for(unsigned mode=0;mode<4;++mode){unsigned csr=0x1f80|(mode<<13);b->writeData(0,start.size(),&start[0],true);_mm_setcsr(csr);Ogre::Mesh::softwareVertexPoseBlend(.35f,positions,normals,&v);expected=b->Bytes();unsigned flags=_mm_getcsr()&63;b->writeData(0,start.size(),&start[0],true);_mm_setcsr(csr);C(pose_cpu_blend::Try(.35f,positions,normals,&v));C(b->Bytes()==expected);C((_mm_getcsr()&63)==flags);C(!b->isLocked()&&hold.useCount()==2);}
 _mm_setcsr(saved);
 const unsigned raw[]={0,0x80000000,1,0x7fc12345,0x7f800000};for(unsigned i=0;i<5;++i){float weight;memcpy(&weight,&raw[i],4);unsigned before=b->uploads;C(!pose_cpu_blend::Try(weight,positions,normals,&v)&&b->uploads==before&&!b->isLocked());}
 {Map tinyEntries;tinyEntries[0]=Ogre::Vector3(1,2,3);C(!pose_cpu_blend::Try(.5f,tinyEntries,tinyEntries,&v));}
 v.vertexStart=1;C(!pose_cpu_blend::Try(.5f,positions,normals,&v));v.vertexStart=0;
 v.vertexDeclaration->removeElement(Ogre::VES_NORMAL);v.vertexDeclaration->addElement(1,12,Ogre::VET_FLOAT3,Ogre::VES_NORMAL);v.vertexBufferBinding->setBinding(1,hold);C(!pose_cpu_blend::Try(.5f,positions,normals,&v));v.vertexDeclaration->removeElement(Ogre::VES_NORMAL);v.vertexDeclaration->addElement(0,12,Ogre::VET_FLOAT3,Ogre::VES_NORMAL);
 b->writeData(0,start.size(),&start[0],true);Ogre::Mesh::softwareVertexPoseBlend(.35f,positions,normals,&v);expected=b->Bytes();b->writeData(0,start.size(),&start[0],true);b->failUnlock=true;bool caught=false;unsigned before=b->uploads;try{pose_cpu_blend::Try(.35f,positions,normals,&v);}catch(const std::runtime_error&e){caught=std::string(e.what())=="late upload";}C(caught&&b->uploads==before+1&&!b->isLocked()&&pose_cpu_blend::busy==0);b->failUnlock=false;b->_updateFromShadow();C(b->Bytes()==expected);
 C(pose_cpu_pool::tasks[0].input==NULL&&pose_cpu_pool::tasks[1].input==NULL);
 {std::vector<unsigned char>cpu(start);Work invalid={.35f,NULL,&normals,&cpu[0],32,count,12,true};unsigned csr=_mm_getcsr();C(!pose_cpu_pool::Execute(&invalid));C(_mm_getcsr()==csr&&pose_cpu_pool::tasks[0].input==NULL&&pose_cpu_pool::tasks[1].input==NULL);}
 pose_cpu_blend::Stop();C(pose_cpu_pool::pool==NULL&&pose_cpu_blend::scratch==NULL&&!pose_cpu_blend::Try(.35f,positions,normals,&v));return 0;
}
#else
static int BlendCases(){return 0;}
#endif
static int Assets(){
 const char*paths[]={"human/human_male.mesh","human/human_female.mesh","stick_person/Stick_person.mesh","stick_person/Hive_Queen.mesh","robot/robot.mesh"};
 for(unsigned k=0;k<5;++k){std::string path="D:/SteamLibrary/steamapps/common/Kenshi/data/character/meshes/";path+=paths[k];std::ifstream*file=new std::ifstream(path.c_str(),std::ios::binary);C(file->good());Ogre::DataStreamPtr stream(OGRE_NEW Ogre::FileStreamDataStream(path,file,false));char name[40];sprintf_s(name,"asset-offline-%u",k);Ogre::MeshPtr mesh=Ogre::MeshManager::getSingleton().createManual(name,"General",NULL);Ogre::MeshSerializer serializer;serializer.importMesh(stream,mesh.get());
  Ogre::VertexData*v=mesh->getNumSubMeshes()?mesh->getSubMesh(0)->vertexData:mesh->sharedVertexData;if(!v)v=mesh->sharedVertexData;C(v!=NULL);const Ogre::VertexElement*e=v->vertexDeclaration->findElementBySemantic(Ogre::VES_POSITION);C(e!=NULL);const Ogre::HardwareVertexBufferSharedPtr&b=v->vertexBufferBinding->getBuffer(e->getSource());size_t maxPos=0,maxNorm=0,eligible=0;
  for(unsigned i=0;i<mesh->getPoseCount();++i){Ogre::Pose*pose=mesh->getPose((unsigned short)i);size_t a=pose->getVertexOffsets().size(),n=pose->getNormals().size();maxPos=(std::max)(maxPos,a);maxNorm=(std::max)(maxNorm,n);if(a+n>=32768&&a*4>=b->getNumVertices()*3&&n*4>=b->getNumVertices()*3&&b->getSizeInBytes()<=(a+n)*16)++eligible;}
  stream.setNull();delete file;printf("ASSET %s vertices=%u stride=%u poses=%u maxPositionEntries=%u maxNormalEntries=%u coarseEligible=%u\n",paths[k],(unsigned)b->getNumVertices(),(unsigned)b->getVertexSize(),mesh->getPoseCount(),(unsigned)maxPos,(unsigned)maxNorm,(unsigned)eligible);
 }
 return 0;
}
int main(int argc,char**argv){setvbuf(stdout,NULL,_IONBF,0);Ogre::Root root("","","");Ogre::DefaultHardwareBufferManager hardware;Cleanup cleanup;if(argc>1&&!strcmp(argv[1],"assets"))return Assets();C(KernelCases()==0);if(argc>1)C(Benchmark()==0);C(BlendCases()==0);pose_cpu_pool::Stop();printf("PASS %u native pose CPU oracle checks\n",checks);return 0;}
