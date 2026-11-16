// Actual Ogre native pose arithmetic; a deterministic GPU sink counts uploads.
#define main ExistingMorphFixtureMain
#include "test_morph_pose_scope.cpp"
#undef main
#include <ogre/OgreHardwareVertexBuffer.h>
class UploadBuffer:public Ogre::HardwareVertexBuffer {
 std::vector<unsigned char>gpu;
protected:
 void*lockImpl(size_t offset,size_t,Ogre::HardwareBuffer::LockOptions){return &gpu[0]+offset;}
 void unlockImpl(){}
public:
 unsigned uploads;bool failUpload,sehUpload;
 UploadBuffer(size_t stride,size_t count):Ogre::HardwareVertexBuffer(NULL,stride,count,Ogre::HardwareBuffer::HBU_STATIC,false,true),gpu(stride*count),uploads(0),failUpload(false),sehUpload(false){}
 void readData(size_t offset,size_t length,void*out){memcpy(out,&gpu[0]+offset,length);}
 void writeData(size_t offset,size_t length,const void*in,bool){memcpy(&gpu[0]+offset,in,length);if(mShadowBuffer)mShadowBuffer->writeData(offset,length,in,true);}
 void _updateFromShadow(){if(mUseShadowBuffer&&mShadowUpdated&&!mSuppressHardwareUpdate){++uploads;if(sehUpload)RaiseException(0xe0421002,0,0,NULL);if(failUpload)throw std::runtime_error("fixture upload failure");}Ogre::HardwareBuffer::_updateFromShadow();}
 void System(bool value){mSystemMemory=value;}
 Ogre::HardwareBuffer*Shadow()const{return mShadowBuffer;}
 void Shadow(Ogre::HardwareBuffer*p){mShadowBuffer=p;}
 bool Suppressed()const{return mSuppressHardwareUpdate;}
 bool Dirty()const{return mShadowUpdated;}
 const std::vector<unsigned char>&Gpu()const{return gpu;}
};
static morph_pose_scope::CloneFn actualNativeClone=NULL;
static Ogre::HardwareVertexBufferSharedPtr observed;
static bool failFinal=false,sehFinal=false,nesting=false,nestedClean=false;static unsigned duringRefs=0;
static void*ShadowClone(void*self,void*out,const void*name,const void*group){
 void*r=actualNativeClone(self,out,name,group);Ogre::MeshPtr&m=*static_cast<Ogre::MeshPtr*>(out);
 Ogre::VertexData*v=m->getSubMesh(0)->vertexData;
 const Ogre::VertexElement*e=v->vertexDeclaration->findElementBySemantic(Ogre::VES_POSITION);
 Ogre::HardwareVertexBufferSharedPtr old=v->vertexBufferBinding->getBuffer(e->getSource());
 UploadBuffer*b=OGRE_NEW UploadBuffer(old->getVertexSize(),old->getNumVertices());observed=Ogre::HardwareVertexBufferSharedPtr(b);
 std::vector<unsigned char>bytes(old->getSizeInBytes());old->readData(0,bytes.size(),&bytes[0]);b->writeData(0,bytes.size(),&bytes[0],true);
 b->failUpload=failFinal;b->sehUpload=sehFinal;v->vertexBufferBinding->setBinding(e->getSource(),observed);return r;
}
static bool uploadBeforeRemove=true;
static void UploadRemoveSpy(Ogre::Mesh*m){if(!observed.isNull()&&held.get()==m)uploadBeforeRemove=uploadBeforeRemove&&!static_cast<UploadBuffer*>(observed.get())->Suppressed();actualRemove(m);}
static void*UploadBody(const void*name,void*entity,void*,bool){
 Ogre::Entity*e=static_cast<Ogre::Entity*>(entity);held=e->getMesh()->clone(*static_cast<const std::string*>(name),"General");
 if(mode==6&&!nesting){nesting=true;Ogre::MeshPtr parentMesh=held;Ogre::HardwareVertexBufferSharedPtr parent=observed;unsigned sourceRefs=e->getMesh().useCount();std::string nestedName="upload-nested-child";
  morph_pose_scope::Create(UploadBody,&nestedName,entity,NULL,false);
  UploadBuffer*child=static_cast<UploadBuffer*>(observed.get());nestedClean=!child->Suppressed()&&child->uploads==1&&observed.useCount()==2&&e->getMesh().useCount()==sourceRefs;
  held=parentMesh;observed=parent;nestedClean=nestedClean&&static_cast<UploadBuffer*>(parent.get())->Suppressed();nesting=false;
 }
 duringRefs=observed.useCount();
 for(unsigned i=0;i<held->getPoseCount();++i){Ogre::Pose*p=held->getPose((unsigned short)i);Ogre::Mesh::softwareVertexPoseBlend(.25f,p->getVertexOffsets(),p->getNormals(),held->getSubMesh(0)->vertexData);}
 if(mode==5){observed->lock(Ogre::HardwareBuffer::HBL_NORMAL);throw std::runtime_error("native body failure");}
 if(mode==2)throw std::runtime_error("native body failure");
 if(mode==3)RaiseException(0xe0421001,0,0,NULL);
 if(mode==4)fixes::g_fixesCfg.morphPoseReuseEnabled=false;
 BoundaryRemove(held.get());return entity;
}
static bool UploadSehCall(const std::string*name,Ogre::Entity*e){bool caught=false;__try{morph_pose_scope::Create(UploadBody,name,e,NULL,false);}__except(GetExceptionCode()==0xe0421001?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){caught=true;}return caught;}
static bool FlushSehCall(const std::string*name,Ogre::Entity*e){bool caught=false;__try{morph_pose_scope::Create(UploadBody,name,e,NULL,false);}__except(GetExceptionCode()==0xe0421002?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){caught=true;}return caught;}
static int UploadCases(Ogre::SceneManager*scene){
 Ogre::MeshPtr source=Model(0,8);Ogre::Entity*e=scene->createEntity(source);
 Ogre::SubMesh*sub=source->createSubMesh();sub->useSharedVertices=false;sub->vertexData=Vertices(5);std::vector<unsigned char>baseline;
 for(unsigned run=0;run<5;++run){fixes::g_fixesCfg.morphPoseReuseEnabled=run!=0;mode=run>=2?run:0;std::string name="upload-case-"+std::string(1,char('0'+run));bool caught=false;
  try{if(mode==3)caught=UploadSehCall(&name,e);else morph_pose_scope::Create(UploadBody,&name,e,NULL,false);}catch(const std::runtime_error&x){caught=true;C(mode==2&&std::string(x.what())=="native body failure");}
  UploadBuffer*b=static_cast<UploadBuffer*>(observed.get());C(b&&!b->Suppressed()&&!b->Dirty());
  if(run==0){C(b->uploads==8);baseline=b->Gpu();}else{C(b->uploads==1);C(b->Gpu()==baseline);}
  C(duringRefs==(run==0?2u:3u)&&observed.useCount()==2);C(caught==(mode==2||mode==3));C(uploadBeforeRemove);C(morph_pose_scope::tracked==NULL&&morph_pose_scope::cloning==NULL);
  observed.setNull();held.setNull();
 }
 mode=6;fixes::g_fixesCfg.morphPoseReuseEnabled=true;std::string nestedName="upload-nested-parent";morph_pose_scope::Create(UploadBody,&nestedName,e,NULL,false);UploadBuffer*parentBuffer=static_cast<UploadBuffer*>(observed.get());C(nestedClean&&!parentBuffer->Suppressed()&&parentBuffer->uploads==1&&parentBuffer->Gpu()==baseline&&observed.useCount()==2);observed.setNull();held.setNull();
 // A final GPU failure must escape a successful native body operation.
 fixes::g_fixesCfg.morphPoseReuseEnabled=true;mode=0;failFinal=true;
 std::string faultName="upload-final-failure";bool caught=false;
 try{morph_pose_scope::Create(UploadBody,&faultName,e,NULL,false);}catch(const std::runtime_error&x){caught=std::string(x.what())=="fixture upload failure";}
 C(caught&&morph_pose_scope::tracked==NULL);UploadBuffer*b=static_cast<UploadBuffer*>(observed.get());C(!b->Suppressed()&&b->Dirty()&&b->uploads==1);
 b->failUpload=false;b->_updateFromShadow();C(!b->Dirty()&&b->Gpu()==baseline);observed.setNull();held.setNull();
 // Cleanup GPU failure must not replace the original native body exception.
 mode=2;faultName="upload-body-and-flush-failure";caught=false;unsigned beforeErrors=errors;
 try{morph_pose_scope::Create(UploadBody,&faultName,e,NULL,false);}catch(const std::runtime_error&x){caught=std::string(x.what())=="native body failure";}
 C(caught&&errors==beforeErrors+1&&morph_pose_scope::tracked==NULL);b=static_cast<UploadBuffer*>(observed.get());C(!b->Suppressed()&&b->Dirty()&&b->uploads==1);
 b->failUpload=false;b->_updateFromShadow();C(b->Gpu()==baseline);observed.setNull();held.setNull();failFinal=false;
 // An abandoned native shadow lock is not forcibly released by our cleanup.
 mode=5;faultName="upload-locked-body-failure";caught=false;beforeErrors=errors;
 try{morph_pose_scope::Create(UploadBody,&faultName,e,NULL,false);}catch(const std::runtime_error&x){caught=std::string(x.what())=="native body failure";}
 b=static_cast<UploadBuffer*>(observed.get());C(caught&&b->isLocked()&&!b->Suppressed()&&errors==beforeErrors+1);b->unlock();C(!b->isLocked()&&b->Gpu()==baseline);observed.setNull();held.setNull();
 // Normal and exceptional structured GPU failures preserve cleanup and fault identity.
 mode=0;sehFinal=true;faultName="upload-seh-flush";C(FlushSehCall(&faultName,e));b=static_cast<UploadBuffer*>(observed.get());C(!b->Suppressed()&&b->uploads==1&&observed.useCount()==2&&morph_pose_scope::tracked==NULL);b->sehUpload=false;b->_updateFromShadow();C(b->Gpu()==baseline);observed.setNull();held.setNull();
 mode=3;faultName="upload-seh-body-and-flush";beforeErrors=errors;C(UploadSehCall(&faultName,e));b=static_cast<UploadBuffer*>(observed.get());C(!b->Suppressed()&&b->uploads==1&&observed.useCount()==2&&errors==beforeErrors+1);b->sehUpload=false;b->_updateFromShadow();C(b->Gpu()==baseline);observed.setNull();held.setNull();sehFinal=false;
 scene->destroyEntity(e);return 0;
}
struct BatchCleanup{morph_upload_batch::Batch*b;~BatchCleanup(){morph_upload_batch::EndNoThrow(b);}};
static int GuardCases(){
 Ogre::MeshPtr source=Model(0,0),target=Model(0,0);morph_upload_batch::Batch batch;morph_upload_batch::Init(&batch);BatchCleanup batchCleanup={&batch};
 morph_upload_batch::Begin(&batch,NULL,target.get());C(!batch.live);morph_upload_batch::Begin(&batch,source.get(),source.get());C(!batch.live);morph_upload_batch::Begin(&batch,source.get(),target.get());C(!batch.live);
 Ogre::SubMesh*ss=source->createSubMesh(),*ts=target->createSubMesh();ss->useSharedVertices=ts->useSharedVertices=false;
 morph_upload_batch::Begin(&batch,source.get(),target.get());C(!batch.live);ss->vertexData=Vertices(5);ts->vertexData=Vertices(5);
 morph_upload_batch::Begin(&batch,source.get(),target.get());C(!batch.live); // native CPU buffer has no shadow
 Ogre::HardwareVertexBufferSharedPtr sb=ss->vertexData->vertexBufferBinding->getBuffer(0);
 UploadBuffer*b=OGRE_NEW UploadBuffer(24,4);Ogre::HardwareVertexBufferSharedPtr hold(b);ts->vertexData->vertexBufferBinding->setBinding(0,hold);
 unsigned refs=hold.useCount();morph_upload_batch::Begin(&batch,source.get(),target.get());C(batch.live&&b->Suppressed()&&hold.useCount()==refs+1);morph_upload_batch::Begin(&batch,source.get(),target.get());C(hold.useCount()==refs+1);morph_upload_batch::End(&batch);C(!batch.live&&!b->Suppressed()&&hold.useCount()==refs);morph_upload_batch::End(&batch);C(hold.useCount()==refs);
 b->suppressHardwareUpdate(true);morph_upload_batch::Begin(&batch,source.get(),target.get());C(!batch.live&&b->Suppressed());b->suppressHardwareUpdate(false);
 b->System(true);morph_upload_batch::Begin(&batch,source.get(),target.get());C(!batch.live);b->System(false);
 b->lock(Ogre::HardwareBuffer::HBL_READ_ONLY);morph_upload_batch::Begin(&batch,source.get(),target.get());C(!batch.live&&b->isLocked());b->unlock();
 Ogre::HardwareBuffer*shadow=b->Shadow();b->Shadow(sb.get());morph_upload_batch::Begin(&batch,source.get(),target.get());C(!batch.live);b->Shadow(shadow);
 ss->vertexData->vertexBufferBinding->setBinding(0,hold);morph_upload_batch::Begin(&batch,source.get(),target.get());C(!batch.live&&!b->Suppressed());ss->vertexData->vertexBufferBinding->setBinding(0,sb);
 ts->vertexData->vertexBufferBinding->unsetBinding(0);morph_upload_batch::Begin(&batch,source.get(),target.get());C(!batch.live);ts->vertexData->vertexBufferBinding->setBinding(0,hold);
 ts->vertexData->vertexDeclaration->removeAllElements();morph_upload_batch::Begin(&batch,source.get(),target.get());C(!batch.live);C(hold.useCount()==refs&&!b->Suppressed());return 0;
}
int main(){SetUnhandledExceptionFilter(Crash);setvbuf(stdout,NULL,_IONBF,0);C(InitSdk()==0);Ogre::Root root("","","");Ogre::DefaultHardwareBufferManager hardware;Cleanup cleanup;
 HMODULE ogre=GetModuleHandleA("OgreMain_x64.dll");C(DiskHash(ogre,"7d3888d21398647519f10a89b1f300d8606f761ad159e6021a797b1b9b3df5f6"));C(AdmissionCases(ogre)==0);
 C(sdkAdd((char*)ogre+0x1a3ee0,(void*)&CloneBridge,(void**)&actualNativeClone)==0);nativeClone=ShadowClone;
 C(sdkAdd((char*)ogre+0x1be970,(void*)&CreateBridge,(void**)&nativeCreate)==0);
 morph_pose_scope::Install(NULL,NULL);C(morph_pose_scope::admitted);nativePose=morph_pose_scope::originalPose;actualRemove=morph_pose_scope::originalRemove;morph_pose_scope::originalRemove=UploadRemoveSpy;
 Ogre::MeshPtr calibration=Model(0,0);BoundaryRemove(calibration.get());morph_pose_scope::gameBase=boundaryReturn-0x71ce2;boundaryFunction=morph_pose_scope::Remove;
 Ogre::MaterialManager::getSingleton().initialise();activeScene=root.createSceneManager(Ogre::ST_GENERIC,1,Ogre::INSTANCING_CULLING_SINGLETHREAD);
 C(GuardCases()==0);C(UploadCases(activeScene)==0);root.destroySceneManager(activeScene);activeScene=NULL;VirtualFree(fakeGame,0,MEM_RELEASE);fakeGame=NULL;printf("PASS %u native morph upload checks\n",checks);return 0;
}
