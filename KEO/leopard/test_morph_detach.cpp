// Exact production Detach body, actual native Mesh/Pose ownership.
#define NOMINMAX
#include <windows.h>
#include <ogre/OgreRoot.h>
#include <ogre/OgreMeshManager.h>
#include <ogre/OgreMesh.h>
#include <ogre/OgrePose.h>
#include <cstdio>
#include <algorithm>
static unsigned checks=0, comparisons=0;
#define CHECK(x) do{++checks;if(!(x)){printf("CHECK line%d %s\n",__LINE__,#x);return 1;}}while(0)
struct Loader:Ogre::ManualResourceLoader{void loadResource(Ogre::Resource*){}};
struct Context {
 bool targetLive,enabled;size_t count;Ogre::Pose*borrowed[512];Ogre::MeshPtr target;
 Ogre::MeshPtr&Target(){return target;}
};
static Ogre::PoseList&Poses(Ogre::Mesh*m){return const_cast<Ogre::PoseList&>(m->getPoseList());}
static bool Match(Ogre::Pose*a,Ogre::Pose*b){++comparisons;return a==b;}
#include "morph_detach_actual.inc"
int main(){
 Ogre::Root root("","","");Loader loader;Ogre::MeshManager&manager=Ogre::MeshManager::getSingleton();
 unsigned normalComparisons=0;
 for(unsigned mode=0;mode<10;++mode){
  char a[64],b[64];sprintf_s(a,"detach-source-%u",mode);sprintf_s(b,"detach-target-%u",mode);
  Ogre::MeshPtr source=manager.createManual(a,"General",&loader),target=manager.createManual(b,"General",&loader);
  Context c;c.targetLive=true;c.enabled=true;c.count=512;c.target=target;
  for(unsigned i=0;i<512;++i)c.borrowed[i]=source->createPose(0,"pose");
  Ogre::Pose*owned=target->createPose(0,"owned");Poses(target.get()).clear();
  for(unsigned i=0;i<512;++i)Poses(target.get()).push_back(c.borrowed[i]);
  if(mode==1)std::reverse(Poses(target.get()).begin(),Poses(target.get()).end());
  if(mode==2)Poses(target.get())[200]=owned; // Same size, one owned replacement.
  if(mode==3)Poses(target.get()).insert(Poses(target.get()).begin()+77,owned);
  if(mode==4)Poses(target.get()).resize(93);
  if(mode==5)Poses(target.get()).clear();
  if(mode==6)c.enabled=false; // Cleanup remains mandatory when disabled.
  if(mode==7)c.count=0;
  if(mode==8)c.targetLive=false;
  if(mode==9)Poses(target.get()).back()=owned; // Last-pointer mismatch.
  comparisons=0;Detach(&c,target.get());
  bool skipped=mode==7||mode==8;
  bool correct=skipped?Poses(target.get()).size()==512:
   ((mode==2||mode==3||mode==9)?Poses(target.get()).size()==1&&Poses(target.get())[0]==owned:Poses(target.get()).empty());
  bool reset=skipped?c.count==(mode==7?0u:512u)&&c.enabled:c.count==0&&!c.enabled;
  unsigned measured=comparisons;bool sourceIntact=source->getPoseCount()==512;
  // Sanitize before assertions so even a rejected mutant shuts native Ogre down.
  Poses(target.get()).clear();Poses(target.get()).push_back(owned);target->removeAllPoses();
  c.target.setNull();manager.remove(target->getHandle());target.setNull();
  manager.remove(source->getHandle());source.setNull();
  CHECK(correct);CHECK(reset);CHECK(sourceIntact);
  if(mode==0){normalComparisons=measured;CHECK(measured==512);}
  if(mode==1)CHECK(measured<=512u*512u+512u);
  if(skipped)CHECK(measured==0);
 }
 printf("PASS %u actual native detach checks; ordered512 comparisons=%u\n",checks,normalComparisons);return 0;
}
