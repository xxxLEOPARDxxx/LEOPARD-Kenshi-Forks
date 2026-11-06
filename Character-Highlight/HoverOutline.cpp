#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include "HoverOutline.h"
#include "HoverOutlineShaders.h"
#include "MaskShaderSource.h"
#include <kenshi/Appearance.h>
#include <kenshi/Character.h>
#include <kenshi/Globals.h>
#include <kenshi/GameWorld.h>
#include <kenshi/Renderer.h>
#include <ogre/OgreRoot.h>
#include <ogre/OgreEntity.h>
#include <ogre/OgreSubEntity.h>
#include <ogre/OgreSceneNode.h>
#include <ogre/OgreSceneManager.h>
#include <ogre/OgreManualObject.h>
#include <ogre/OgreCamera.h>
#include <ogre/OgreRenderTargetListener.h>
#include <ogre/Math/Simple/OgreAabb.h>
#include <ogre/OgreHighLevelGpuProgramManager.h>
#include <ogre/OgreHighLevelGpuProgram.h>
#include <ogre/OgreResourceGroupManager.h>
#include <ogre/OgreMaterialManager.h>
#include <ogre/OgreTextureManager.h>
#include <ogre/OgreHardwarePixelBuffer.h>
#include <ogre/OgreRenderTexture.h>
#include <ogre/OgreTextureUnitState.h>
#include <ogre/OgreTechnique.h>
#include <ogre/OgrePass.h>
#include <ogre/Compositor/OgreCompositorWorkspace.h>
#include <ogre/Compositor/OgreCompositorWorkspaceListener.h>
#include <ogre/Compositor/OgreCompositorWorkspaceDef.h>
#include <ogre/Compositor/OgreCompositorManager2.h>
#include <ogre/Compositor/OgreCompositorNodeDef.h>
#include <ogre/Compositor/Pass/OgreCompositorPass.h>
#include <ogre/Compositor/Pass/PassClear/OgreCompositorPassClearDef.h>
#include <ogre/Compositor/Pass/PassScene/OgreCompositorPassSceneDef.h>
#include <Debug.h>
#include <map>
#include <vector>
#include <sstream>
#include <stdexcept>

namespace HoverOutline {
namespace {
const unsigned kActors=512,kMaterials=2048,kEntities=64,kBindings=128;
unsigned frame=0,names=0,materialCount=0,createsLeft=0;
bool failed=false,defined=false,callbacksReported=false;
unsigned stage=0;
DWORD faultCode=0;void* faultAddress=NULL;ULONG_PTR faultAccess=0;
LONG RecordFault(EXCEPTION_POINTERS* info){faultCode=info->ExceptionRecord->ExceptionCode;faultAddress=info->ExceptionRecord->ExceptionAddress;faultAccess=info->ExceptionRecord->NumberParameters>1?info->ExceptionRecord->ExceptionInformation[1]:0;return EXCEPTION_EXECUTE_HANDLER;}
bool debug=false;
unsigned draws=0;
struct Key {
 unsigned v[5];explicit Key(const hand& h){v[0]=h.type;v[1]=h.container;v[2]=h.containerSerial;v[3]=h.index;v[4]=h.serial;}
 bool operator<(const Key& b)const{for(unsigned i=0;i<5;++i)if(v[i]!=b.v[i])return v[i]<b.v[i];return false;}
};
struct Binding {
 Ogre::SubEntity* address; // Comparison key only; a current live entity is enumerated each draw.
 Ogre::MaterialPtr original,mask;unsigned seen;
 Ogre::ColourValue colour;bool unsupported;
 Binding():address(NULL),seen(0),colour(-1,-1,-1,-1),unsupported(false){}
};
struct Actor {hand who;unsigned seen;Ogre::ColourValue colour;std::vector<Binding> bindings;Actor():seen(0){}};
typedef std::map<Key,Actor> Actors;Actors actors;
typedef std::map<std::string,Ogre::HighLevelGpuProgramPtr> Programs;Programs programs;
Ogre::TexturePtr maskTexture;
Ogre::MaterialPtr edgeMaterial;
Ogre::ManualObject* quad=NULL;
Ogre::SceneNode* quadNode=NULL;
Ogre::SceneManager* scene=NULL;
Ogre::Camera* camera=NULL;
Ogre::CompositorWorkspace* maskWorkspace=NULL;
Ogre::CompositorWorkspace* mainWorkspace=NULL;
Ogre::CompositorWorkspace* surfaceWorkspace=NULL;
unsigned width=0,height=0;
float outlineWidth=2,outlineBrightness=1;
void RenderMask(Ogre::Viewport* viewport);
void UpdateMask();
void UpdateWorkspace(Ogre::CompositorWorkspace* ws){
 __try {ws->_update();}
 __finally {ws->_endUpdate(false);}
}
struct MaskListener:Ogre::RenderTargetListener {
 void postViewportUpdate(const Ogre::RenderTargetViewportEvent& event){RenderMask(event.source);}
} maskListener;
struct MainListener:Ogre::CompositorWorkspaceListener {
 Ogre::CompositorWorkspaceListener* prior;
 MainListener():prior(NULL){}
 void workspacePreUpdate(Ogre::CompositorWorkspace* workspace){
  if(prior && prior!=this)prior->workspacePreUpdate(workspace);
  if(workspace==mainWorkspace)UpdateMask();
 }
 void passPreExecute(Ogre::CompositorPass* pass){if(prior && prior!=this)prior->passPreExecute(pass);}
};
std::map<Ogre::CompositorWorkspace*,MainListener> listeners;
std::string Unique(const char* name){std::ostringstream s;s<<"LEOPARD_CH_"<<name<<++names;return s.str();}
void LogFailure(const char* why){if(!failed)ErrorLog(std::string("Character Highlight mask: ")+why);failed=true;if(quad)quad->setVisible(false);}
unsigned CollectImpl(Character* c,Ogre::Entity** entities){
 if(!c || !c->getAppearance() || !c->getAppearance()->body)return 0;
 Ogre::Entity* body=c->getAppearance()->body;
 if(!body->isVisible() || !camera->isVisible(Ogre::AxisAlignedBox(body->getWorldAabb().getMinimum(),body->getWorldAabb().getMaximum())))return 0;
 unsigned count=1;entities[0]=body;
 Ogre::Node* nodes[128];unsigned n=1;nodes[0]=c->getAppearance()->getSceneNode();
 for(unsigned i=0;i<n && count<kEntities;++i){
  Ogre::Node* node=nodes[i];if(!node)continue;
  Ogre::SceneNode* s=dynamic_cast<Ogre::SceneNode*>(node);
  if(s)for(unsigned j=0;j<s->numAttachedObjects() && j<kEntities;++j){
   Ogre::MovableObject* o=s->getAttachedObject(j);if(!o || o->getMovableType()!="Entity")continue;
   Ogre::Entity* e=static_cast<Ogre::Entity*>(o);bool seen=false;
   for(unsigned k=0;k<count;++k)if(entities[k]==e){seen=true;break;}
   if(!seen && count<kEntities)entities[count++]=e;
  }
  for(unsigned j=0;j<node->numChildren() && n<128;++j){
   Ogre::Node* child=node->getChild(j);bool seen=false;
   for(unsigned k=0;k<n;++k)if(nodes[k]==child){seen=true;break;}
   if(!seen)nodes[n++]=child;
  }
 }
 return count;
}
unsigned Collect(Character* c,Ogre::Entity** out){__try{return CollectImpl(c,out);}__except(EXCEPTION_EXECUTE_HANDLER){return 0;}}
void Release(Binding& b){
 if(!b.mask.isNull()){Ogre::MaterialManager::getSingleton().remove(b.mask->getHandle());b.mask.setNull();if(materialCount)--materialCount;}
 b.original.setNull();b.unsupported=false;b.colour=Ogre::ColourValue(-1,-1,-1,-1);
}
std::string ReadResource(const std::string& name,const std::string& group){
 return Ogre::ResourceGroupManager::getSingleton().openResource(name,group)->getAsString();
}
Ogre::HighLevelGpuProgramPtr Program(const std::string& name,const std::string& source,Ogre::GpuProgramType type,const std::string& entry,const std::string& defines){
 Ogre::HighLevelGpuProgramPtr p=Ogre::HighLevelGpuProgramManager::getSingleton().createProgram(name,Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,"hlsl",type);
 p->setSource(source);p->setParameter("entry_point",entry);p->setParameter("target",type==Ogre::GPT_VERTEX_PROGRAM?"vs_4_0":"ps_4_0");
 p->setParameter("column_major_matrices","false");p->setParameter("enable_backwards_compatibility","true");if(!defines.empty())p->setParameter("preprocessor_defines",defines);
 p->load();if(p->hasCompileError())throw std::runtime_error("mask shader compilation failed");return p;
}
Ogre::HighLevelGpuProgramPtr MaskProgram(const Ogre::GpuProgramPtr& original){
 const std::string key=original->getName();Programs::iterator i=programs.find(key);if(i!=programs.end())return i->second;
 if(programs.size()>=64)throw std::runtime_error("mask shader cache limit");
 std::string source=original->getSource();
 if(source.empty())source=ReadResource(original->getSourceFile(),original->getGroup());
 source=CharacterMaskSource::Build(source,ReadResource("gbuffer.hlsl",original->getGroup()));
 Ogre::HighLevelGpuProgramPtr p=Program(Unique("MaskFP"),source,Ogre::GPT_FRAGMENT_PROGRAM,original->getParameter("entry_point"),original->getParameter("preprocessor_defines"));
 programs.insert(std::make_pair(key,p));return p;
}
void Attach(Binding& b,Ogre::SubEntity* sub){
 b.original=sub->getMaterial();b.unsupported=true;
 if(b.original.isNull() || !b.original->getNumTechniques())return;
 Ogre::Technique* t=b.original->getTechnique(0);if(!t->getNumPasses())return;
 Ogre::Pass* base=t->getPass(0);if(!base->hasVertexProgram() || !base->hasFragmentProgram())return;
 Ogre::HighLevelGpuProgramPtr fp=MaskProgram(base->getFragmentProgram());
 b.mask=b.original->clone(Unique("MaskMaterial"));++materialCount;
 Ogre::Pass* p=b.mask->getTechnique(0)->getPass(0);
 p->setFragmentProgram(fp->getName());
 p->getFragmentProgramParameters()->copyMatchingNamedConstantsFrom(*base->getFragmentProgramParameters());
 p->setDepthCheckEnabled(true);p->setDepthWriteEnabled(true);p->setSceneBlending(Ogre::SBT_REPLACE);
 // The original vertex program, native bone auto-constants, UV/part masks and
 // original fragment alpha-discard remain intact. No pass is added to the model.
 if(p->getVertexProgramParameters()->_findNamedConstantDefinition("overrideDepth",false))p->getVertexProgramParameters()->setNamedConstant("overrideDepth",false);
 b.unsupported=false;
}
void UpdateVertexMask(Ogre::Pass* to,Ogre::Pass* from){
 Ogre::GpuProgramParametersSharedPtr src=from->getVertexProgramParameters(),dst=to->getVertexProgramParameters();
 const Ogre::GpuConstantDefinition* d=src->_findNamedConstantDefinition("hiddenMask",false);
 if(d)dst->setNamedConstant("hiddenMask",*src->getIntPointer(d->physicalIndex));
 d=src->_findNamedConstantDefinition("hideSide",false);
 if(d){const float* p=src->getFloatPointer(d->physicalIndex);dst->setNamedConstant("hideSide",Ogre::Vector2(p[0],p[1]));}
}
void RenderActor(Actor& a,Ogre::Viewport* viewport){
 Ogre::Entity* entities[kEntities];unsigned count=Collect(a.who.getCharacter(),entities);
 for(unsigned i=0;i<count;++i){
  Ogre::Entity* e=entities[i];if(!e->isVisible() || !camera->isVisible(Ogre::AxisAlignedBox(e->getWorldAabb().getMinimum(),e->getWorldAabb().getMaximum())))continue;
  for(unsigned j=0;j<e->getNumSubEntities() && j<kBindings;++j){
   Ogre::SubEntity* sub=e->getSubEntity(j);
   unsigned k=0;for(;k<a.bindings.size();++k)if(a.bindings[k].address==sub)break;
   if(k==a.bindings.size()){if(a.bindings.size()>=kBindings)continue;a.bindings.push_back(Binding());a.bindings.back().address=sub;}
   Binding& b=a.bindings[k];b.seen=frame;
   if(!b.original.isNull() && sub->getMaterial()!=b.original)Release(b);
   if(b.mask.isNull()){
    if(viewport)continue;
    if(b.unsupported && sub->getMaterial()==b.original)continue;
    if(!createsLeft || materialCount>=kMaterials)continue;--createsLeft;Attach(b,sub);
   }
   if(b.mask.isNull())continue;
   Ogre::Pass* p=b.mask->getTechnique(0)->getPass(0);UpdateVertexMask(p,b.original->getTechnique(0)->getPass(0));
   if(b.colour!=a.colour){p->getFragmentProgramParameters()->setNamedConstant("leopardMaskColour",a.colour);b.colour=a.colour;}
   if(viewport){stage=5;scene->_injectRenderWithPass(p,sub,NULL,false,false);++draws;stage=4;}
  }
 }
 for(unsigned k=0;k<a.bindings.size();)if(a.bindings[k].seen!=frame){Release(a.bindings[k]);a.bindings.erase(a.bindings.begin()+k);}else ++k;
}
void RenderMask(Ogre::Viewport* viewport){
 stage=4;
 for(Actors::iterator i=actors.begin();i!=actors.end();++i)RenderActor(i->second,viewport);
 callbacksReported=true;
}
void DestroySurface(){
 if(maskWorkspace){Ogre::Root::getSingleton().getCompositorManager2()->removeWorkspace(maskWorkspace);maskWorkspace=NULL;}
 if(quad){quadNode->detachObject(quad);scene->destroyManualObject(quad);quad=NULL;}
 if(quadNode){scene->destroySceneNode(quadNode);quadNode=NULL;}
 if(!edgeMaterial.isNull()){Ogre::MaterialManager::getSingleton().remove(edgeMaterial->getHandle());edgeMaterial.setNull();}
 if(!maskTexture.isNull()){Ogre::TextureManager::getSingleton().remove(maskTexture->getHandle());maskTexture.setNull();}
}
void EnsureSurface(Renderer* renderer){
 const unsigned w=static_cast<unsigned>(renderer->getScreenWidth()),h=static_cast<unsigned>(renderer->getScreenHeight());
 if(maskWorkspace && w==width && h==height && renderer->getCamera()==camera && renderer->getSceneManager()==scene && mainWorkspace==surfaceWorkspace)return;
 DestroySurface();surfaceWorkspace=mainWorkspace;scene=renderer->getSceneManager();camera=renderer->getCamera();width=w;height=h;
 if(!width || !height || width>16384 || height>16384)throw std::runtime_error("invalid render size");
 Ogre::CompositorManager2* m=renderer->getRoot()->getCompositorManager2();
 if(!defined){
  Ogre::CompositorNodeDef* node=m->addNodeDefinition("LEOPARD_CH_MaskNode");
  node->addTextureSourceName("output",0,Ogre::TextureDefinitionBase::TEXTURE_INPUT);node->setNumTargetPass(1);
  Ogre::CompositorTargetDef* target=node->addTargetPass("output");target->setNumPasses(2);
  Ogre::CompositorPassClearDef* clear=static_cast<Ogre::CompositorPassClearDef*>(target->addPass(Ogre::PASS_CLEAR));clear->mColourValue=Ogre::ColourValue(0,0,0,0);
  Ogre::CompositorPassSceneDef* draw=static_cast<Ogre::CompositorPassSceneDef*>(target->addPass(Ogre::PASS_SCENE));
  draw->mVisibilityMask=0;draw->mFirstRQ=draw->mLastRQ=0;draw->mUpdateLodLists=false;draw->mIncludeOverlays=false;
  Ogre::CompositorWorkspaceDef* def=m->addWorkspaceDefinition(Ogre::IdString("LEOPARD_CH_MaskWorkspace"));def->connectOutput(Ogre::IdString("LEOPARD_CH_MaskNode"),0);defined=true;
 }
 maskTexture=Ogre::TextureManager::getSingleton().createManual(Unique("MaskTexture"),Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,Ogre::TEX_TYPE_2D,(width+1)/2,(height+1)/2,0,Ogre::PF_FLOAT16_RGBA,Ogre::TU_RENDERTARGET);
 Ogre::RenderTarget* rt=maskTexture->getBuffer()->getRenderTarget();
 maskWorkspace=m->addWorkspace(scene,rt,camera,Ogre::IdString("LEOPARD_CH_MaskWorkspace"),false);rt->addListener(&maskListener);
 Ogre::HighLevelGpuProgramPtr vp=Ogre::HighLevelGpuProgramManager::getSingleton().getByName("LEOPARD_CH_EdgeVP"),fp=Ogre::HighLevelGpuProgramManager::getSingleton().getByName("LEOPARD_CH_EdgeFP");
 if(vp.isNull())vp=Program("LEOPARD_CH_EdgeVP",HoverOutlineShaders::vertex,Ogre::GPT_VERTEX_PROGRAM,"main_vs","");
 if(fp.isNull())fp=Program("LEOPARD_CH_EdgeFP",HoverOutlineShaders::fragment,Ogre::GPT_FRAGMENT_PROGRAM,"main_fs","");
 edgeMaterial=Ogre::MaterialManager::getSingleton().create(Unique("EdgeMaterial"),Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
 Ogre::Pass* pass=edgeMaterial->getTechnique(0)->getPass(0);pass->setVertexProgram(vp->getName());pass->setFragmentProgram(fp->getName());
 pass->setDepthCheckEnabled(false);pass->setDepthWriteEnabled(false);pass->setSceneBlending(Ogre::SBT_TRANSPARENT_ALPHA);pass->setCullingMode(Ogre::CULL_NONE);
 Ogre::TextureUnitState* tex=pass->createTextureUnitState(maskTexture->getName());tex->setTextureAddressingMode(Ogre::TextureUnitState::TAM_CLAMP);tex->setTextureFiltering(Ogre::TFO_NONE);
 const Ogre::CompositorChannel& gbuffer=mainWorkspace->getGlobalTexture(Ogre::IdString("global_gbuffer"));if(gbuffer.textures.size()<3)throw std::runtime_error("main scene depth unavailable");
 tex=pass->createTextureUnitState(gbuffer.textures[2]->getName());tex->setTextureAddressingMode(Ogre::TextureUnitState::TAM_CLAMP);tex->setTextureFiltering(Ogre::TFO_NONE);
 pass->getFragmentProgramParameters()->setNamedConstant("pixel",Ogre::Vector2(1.f/width,1.f/height));
 quad=scene->createManualObject();quad->begin(edgeMaterial->getName(),Ogre::RenderOperation::OT_TRIANGLE_LIST);
 quad->position(-1,1,0);quad->textureCoord(0,0);quad->position(-1,-1,0);quad->textureCoord(0,1);
 quad->position(1,-1,0);quad->textureCoord(1,1);quad->position(1,1,0);quad->textureCoord(1,0);quad->quad(0,1,2,3);quad->end();
 quad->setRenderQueueGroup(88);quad->setLocalAabb(Ogre::Aabb::BOX_INFINITE);
 quadNode=scene->getRootSceneNode()->createChildSceneNode();quadNode->attachObject(quad);quad->setVisible(false);
}
void UpdateMaskImpl(){
 if(failed || actors.empty()){if(quad)quad->setVisible(false);return;}
 if(!ou || !ou->render)return;Renderer* renderer=static_cast<Renderer*>(ou->render);
 stage=1;EnsureSurface(renderer);createsLeft=8;callbacksReported=false;stage=2;
 // All shader compilation/allocation happens before entering workspace update.
 for(Actors::iterator i=actors.begin();i!=actors.end();++i)RenderActor(i->second,NULL);
 stage=3;maskWorkspace->_beginUpdate(false);
 UpdateWorkspace(maskWorkspace);stage=6;
 if(!callbacksReported)throw std::runtime_error("mask scene callback not invoked");
 Ogre::GpuProgramParametersSharedPtr p=edgeMaterial->getTechnique(0)->getPass(0)->getFragmentProgramParameters();
 p->setNamedConstant("radius",outlineWidth);p->setNamedConstant("brightness",outlineBrightness);quad->setVisible(true);
}
bool UpdateNative(){__try{UpdateMaskImpl();return true;}__except(RecordFault(GetExceptionInformation())){return false;}}
void UpdateMask(){
 LARGE_INTEGER begin,end,freq;if(debug)QueryPerformanceCounter(&begin);draws=0;
 try{if(!UpdateNative()){
 HMODULE mod=NULL;char path[MAX_PATH]={0},note[420];
 GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<const char*>(faultAddress),&mod);
 if(mod)GetModuleFileNameA(mod,path,MAX_PATH);const char* name=strrchr(path,'\\');name=name?name+1:path;
 sprintf_s(note,"native fault code=%08lX stage=%u %s+0x%llX access=0x%llX",faultCode,stage,name,reinterpret_cast<ULONG_PTR>(faultAddress)-reinterpret_cast<ULONG_PTR>(mod),faultAccess);LogFailure(note);
 }}catch(const std::exception& e){LogFailure(e.what());}catch(...){LogFailure("render failure");}
 if(debug && !actors.empty()){
  QueryPerformanceCounter(&end);QueryPerformanceFrequency(&freq);
  static double total=0,maximum=0;static unsigned samples=0;static DWORD last=0;
  const double ms=(end.QuadPart-begin.QuadPart)*1000.0/freq.QuadPart;total+=ms;if(ms>maximum)maximum=ms;++samples;
  DWORD now=GetTickCount();if(now-last>=10000){
   char note[240];sprintf_s(note,"Character Highlight mask: actors=%u materials=%u programs=%u draws=%u size=%ux%u CPU=%.3fms mean %.3fms max",static_cast<unsigned>(actors.size()),materialCount,static_cast<unsigned>(programs.size()),draws,width/2,height/2,total/samples,maximum);
   DebugLog(note);last=now;total=maximum=0;samples=0;
  }
 }
}
void ClearImpl(){
 if(quad)quad->setVisible(false);
 for(Actors::iterator i=actors.begin();i!=actors.end();++i)for(unsigned k=0;k<i->second.bindings.size();++k)Release(i->second.bindings[k]);actors.clear();
}
}
void BeginFrame(){if(++frame==0){Clear();frame=1;}}
void SetDebug(bool enabled){debug=enabled;}
void Draw(Character* c,const Ogre::ColourValue& colour,float w,float brightness){
 if(!c || failed || colour.a<=0 || w<=0 || brightness<=0)return;
 Actors::iterator i=actors.find(Key(c->handle));if(i==actors.end()){if(actors.size()>=kActors)return;i=actors.insert(std::make_pair(Key(c->handle),Actor())).first;i->second.who=c->handle;}
 i->second.seen=frame;i->second.colour=colour;outlineWidth=w;outlineBrightness=brightness;
}
void EndFrame(){
 try{
  for(Actors::iterator i=actors.begin();i!=actors.end();)if(i->second.seen!=frame){for(unsigned k=0;k<i->second.bindings.size();++k)Release(i->second.bindings[k]);actors.erase(i++);}else ++i;
  if(actors.empty()){if(quad)quad->setVisible(false);return;}
  Renderer* r=ou?static_cast<Renderer*>(ou->render):NULL;Ogre::CompositorWorkspace* ws=r?r->getWorkspace():NULL;
  if(ws){
   std::map<Ogre::CompositorWorkspace*,MainListener>::iterator found=listeners.find(ws);
   if(found==listeners.end()){
    if(listeners.size()>=16)throw std::runtime_error("workspace listener limit");
    found=listeners.insert(std::make_pair(ws,MainListener())).first;
    found->second.prior=ws->getListener();ws->setListener(&found->second);
   }
   // Never re-wrap a foreign replacement: it may already forward to us.
   // Recapturing it as our prior would create a C -> foreign -> C cycle.
   mainWorkspace=ws;
  }
 }catch(...){LogFailure("renderer registration/cleanup failed");}
}
void Remove(Character* c){if(!c)return;Actors::iterator i=actors.find(Key(c->handle));if(i!=actors.end()){for(unsigned k=0;k<i->second.bindings.size();++k)Release(i->second.bindings[k]);actors.erase(i);}}
void Clear(){try{ClearImpl();}catch(...){LogFailure("cleanup failed");}}
void Shutdown(){
 if(!quad && !maskWorkspace && actors.empty() && programs.empty())return;
 try{
  ClearImpl();DestroySurface();
  for(Programs::iterator i=programs.begin();i!=programs.end();++i)Ogre::HighLevelGpuProgramManager::getSingleton().remove(i->second->getHandle());
  programs.clear();
  Ogre::HighLevelGpuProgramManager& manager=Ogre::HighLevelGpuProgramManager::getSingleton();
  if(!manager.getByName("LEOPARD_CH_EdgeVP").isNull())manager.remove("LEOPARD_CH_EdgeVP");
  if(!manager.getByName("LEOPARD_CH_EdgeFP").isNull())manager.remove("LEOPARD_CH_EdgeFP");
  Renderer* r=ou?static_cast<Renderer*>(ou->render):NULL;
  Ogre::CompositorWorkspace* current=r?r->getWorkspace():NULL;
  std::map<Ogre::CompositorWorkspace*,MainListener>::iterator i=listeners.find(current);
  if(i!=listeners.end() && current->getListener()==&i->second)current->setListener(i->second.prior);
  mainWorkspace=NULL;surfaceWorkspace=NULL;camera=NULL;scene=NULL;defined=false;
 }catch(...){LogFailure("renderer shutdown cleanup failed");}
}
void PortraitBegin(Character*){} // Queue88 is absent from the native portrait workspace.
void PortraitEnd(){}
}
