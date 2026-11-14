// Included by the production-wrapper fixture, after its native ownership stubs.
static float seenWeight=0;static bool seenUnique=false,seenShaved=false;
static void*seenFourth=NULL;
static void* NativeMorphLookup(void*self,void*data,void*entity,bool unique,float weight,bool shaved){
 seenSecond=data;seenThird=entity;seenUnique=unique;seenWeight=weight;seenShaved=shaved;Native(self);return reinterpret_cast<void*>(0x8877);
}
static void* NativeMorphCreate(const void*name,void*entity,void*data,bool shaved){
 seenSecond=entity;seenThird=data;seenShaved=shaved;Native(const_cast<void*>(name));return reinterpret_cast<void*>(0x8877);
}
static void* NativeMorphSwitch(void*entity,void*owned){seenSecond=owned;Native(entity);return reinterpret_cast<void*>(0x8877);}
static void NativeAnimationBody(void*self,void*entity){seenSecond=entity;Native(self);}
static void NativePoseBlend(float weight,const void*offsets,const void*normals,void*target){
 seenWeight=weight;seenSecond=const_cast<void*>(offsets);seenThird=const_cast<void*>(normals);seenFourth=target;Native(NULL);
}
static void CallAppearanceSeam(int which,void*self,void*data,void*entity){
 using namespace resource_load_probe_detail;
 switch(which){
 case 0:GetMorphedBodyMesh(self,data,entity,true,-0.375f,false);break;
 case 1:CreateMorphedMesh(self,entity,data,true);break;
 case 2:SwitchBodyMesh(self,data);break;
 case 3:SetBodyEntity(self,entity);break;
 case 4:BuildAttachments(self);break;
 case 5:PoseBlend(-0.375f,self,data,entity);break;
 }
}
static bool CatchAppearanceSeh(int which,void*self,void*data,void*entity){
 __try{CallAppearanceSeam(which,self,data,entity);}
 __except(GetExceptionCode()==0xE0426E01?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return true;}
 return false;
}
static void* NativeSwitchOwned(void*self,Owned16 resource){ownedSeen=resource.a^resource.b;Native(self);return reinterpret_cast<void*>(0x8877);}
static void AppearanceNativeTree(void*self){
 using namespace resource_load_probe_detail;
 nativeDuration=2000;GetMorphedBodyMesh(self,self,self,false,0.125f,true);
 nativeDuration=3000;CreateMorphedMesh(self,self,self,false);
 nativeDuration=4000;SwitchBodyMesh(self,self);
 nativeDuration=5000;SetBodyEntity(self,self);
 nativeDuration=6000;BuildAttachments(self);
 nativeDuration=7000;PoseBlend(0.125f,self,self,self);
}
static void ExerciseAppearancePreparation(void*self,void*data){
 using namespace resource_load_probe_detail;
 void*entity=reinterpret_cast<void*>(0x9988);void*expected=reinterpret_cast<void*>(0x8877);
 originalMorphLookup=&NativeMorphLookup;originalMorphCreate=&NativeMorphCreate;originalMorphSwitch=&NativeMorphSwitch;
 originalAnimationBody=&NativeAnimationBody;originalAttachmentBuild=&Native;originalPoseBlend=&NativePoseBlend;
 nativeDuration=23000;int before=nativeCalls;
 Check(GetMorphedBodyMesh(self,data,entity,true,-0.375f,false)==expected&&seen==self&&seenSecond==data&&seenThird==entity&&seenUnique&&!seenShaved&&seenWeight==-0.375f&&nativeCalls==before+1,"morph lookup six total arguments including stack float/bool and replacement result exact");
 GetMorphedBodyMesh(self,data,entity,false,0.875f,true);
 Check(!seenUnique&&seenShaved&&seenWeight==0.875f&&morphLookup.calls==2,"morph lookup opposite bools independent positive weight");
 Check(CreateMorphedMesh(self,entity,data,true)==expected&&seen==self&&seenSecond==entity&&seenThird==data&&seenShaved,"static morph create borrowed string first and result exact");
 Check(SwitchBodyMesh(self,data)==expected&&seen==self&&seenSecond==data,"switch body raw owned16 address and result exact");
 SetBodyEntity(self,entity);Check(seen==self&&seenSecond==entity,"animation body this/entity exact");
 BuildAttachments(self);Check(seen==self,"attachment build exact this");
 PoseBlend(-0.375f,self,data,entity);Check(seen==NULL&&seenWeight==-0.375f&&seenSecond==self&&seenThird==data&&seenFourth==entity,"static pose blend XMM0 and remaining pointer registers exact");
 Stats*stats[6]={&morphLookup,&morphCreate,&morphSwitch,&animationBody,&attachmentBuild,&poseBlend};
 for(int i=0;i<6;++i){
  unsigned __int64 count=stats[i]->calls,total=stats[i]->totalUs;
  Check(count==(i==0?2:1)&&total==23000*count&&stats[i]->over20ms==count,"appearance stage completed elapsed independent");
  enabled=false;int c=clocks,m=mainChecks;before=nativeCalls;CallAppearanceSeam(i,self,data,entity);
  Check(nativeCalls==before+1&&clocks==c&&mainChecks==m&&stats[i]->calls==count,"appearance debug OFF forwards once without timer/thread query");enabled=true;
  onMain=false;c=clocks;before=nativeCalls;CallAppearanceSeam(i,self,data,entity);
  Check(nativeCalls==before+1&&clocks==c&&stats[i]->calls==count,"appearance off-owner forwards without clocks/stats");onMain=true;
  throwsNative=true;c=clocks;before=nativeCalls;bool caught=false;
  try{CallAppearanceSeam(i,self,data,entity);}catch(int v){caught=v==17;}
  Check(caught&&nativeCalls==before+1&&clocks==c+1&&stats[i]->calls==count,"appearance native C++ exception propagates and excludes incomplete sample");throwsNative=false;
  sehNative=true;c=clocks;before=nativeCalls;
  Check(CatchAppearanceSeh(i,self,data,entity)&&nativeCalls==before+1&&clocks==c+1&&stats[i]->calls==count,"appearance native SEH propagates and excludes incomplete sample");sehNative=false;
 }
 originalMorphSwitch=reinterpret_cast<MorphSwitchFn>(&NativeSwitchOwned);
 typedef void*(*OwnedSwitch)(void*,Owned16);Owned16 value;int copies=ownedCopies,drops=ownedDrops;
 Check(reinterpret_cast<OwnedSwitch>(&SwitchBodyMesh)(self,value)==expected&&ownedCopies==copies+1&&ownedDrops==drops+1&&ownedSeen==(value.a^value.b),"switch body native-owned16 copied/destroyed once and result preserved");
 throwsNative=true;copies=ownedCopies;drops=ownedDrops;bool caught=false;
 try{reinterpret_cast<OwnedSwitch>(&SwitchBodyMesh)(self,value);}catch(int v){caught=v==17;}
 Check(caught&&ownedCopies==copies+1&&ownedDrops==drops+1,"switch body native-owned16 unwind exactly once");throwsNative=false;originalMorphSwitch=&NativeMorphSwitch;
 slowRead=slowCount=0;originalResource=&AppearanceNativeTree;Resource(self);
 Check(slowCount==1&&slowUpdates[slowRead].elapsedUs==27000&&slowUpdates[slowRead].children.morphLookup==2000&&slowUpdates[slowRead].children.morphCreate==3000&&slowUpdates[slowRead].children.morphSwitch==4000&&slowUpdates[slowRead].children.animationBody==5000&&slowUpdates[slowRead].children.attachmentBuild==6000&&slowUpdates[slowRead].children.poseBlend==7000,"one slow-update record attributes all six production preparation stages");
 // Leave the existing slow-ring fixture/report expectations intact.
 slowRead=slowCount=0;originalResource=&Native;
}
