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
#include <ogre/OgreTagPoint.h>
#include <ogre/OgreOldSkeletonInstance.h>
#include <ogre/OgreSceneNode.h>
#include <ogre/OgreSceneManager.h>
#include <ogre/OgreManualObject.h>
#include <ogre/OgreCamera.h>
#include <ogre/OgreRenderTargetListener.h>
#include <ogre/Math/Simple/OgreAabb.h>
#include <ogre/OgreHighLevelGpuProgramManager.h>
#include <ogre/OgreHighLevelGpuProgram.h>
#include <ogre/OgreUnifiedHighLevelGpuProgram.h>
#include <ogre/OgreResourceGroupManager.h>
#include <ogre/OgreMaterialManager.h>
#include <ogre/OgreTextureManager.h>
#include <ogre/OgreHardwarePixelBuffer.h>
#include <ogre/OgreImage.h>
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
#include <cmath>
#include <cstring>
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
float lastFarClip=-1;
LONG RecordFault(EXCEPTION_POINTERS* info){
 if(info->ExceptionRecord->ExceptionCode==0xE06D7363)return EXCEPTION_CONTINUE_SEARCH;
 faultCode=info->ExceptionRecord->ExceptionCode;faultAddress=info->ExceptionRecord->ExceptionAddress;faultAccess=info->ExceptionRecord->NumberParameters>1?info->ExceptionRecord->ExceptionInformation[1]:0;return EXCEPTION_EXECUTE_HANDLER;
}
bool debug=false;
unsigned draws=0;
struct Key {
 unsigned v[5];explicit Key(const hand& h){v[0]=h.type;v[1]=h.container;v[2]=h.containerSerial;v[3]=h.index;v[4]=h.serial;}
 bool operator<(const Key& b)const{for(unsigned i=0;i<5;++i)if(v[i]!=b.v[i])return v[i]<b.v[i];return false;}
};
struct Binding {
 Ogre::SubEntity* address; // Comparison key only; a current live entity is enumerated each draw.
 Ogre::MaterialPtr original,mask;unsigned seen;
 Ogre::ColourValue colour;bool unsupported;bool hasFarClip;
 Binding():address(NULL),seen(0),colour(-1,-1,-1,-1),unsupported(false),hasFarClip(false){}
};
struct Actor {hand who;unsigned seen;Ogre::ColourValue colour;Ogre::AxisAlignedBox bounds;std::vector<Binding> bindings;Actor():seen(0){bounds.setNull();}};
typedef std::map<Key,Actor> Actors;Actors actors;
typedef std::map<std::string,Ogre::HighLevelGpuProgramPtr> Programs;Programs programs;
struct ScreenRect{int left,top,right,bottom;};
std::vector<ScreenRect> rects;
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
 // Bone-attached equipment and prosthetics hang on skeleton tag points of the
 // body entity, not on the appearance node tree walked above. Tag points are
 // old bones of the body's skeleton instance; the bone walk avoids any
 // cross-DLL STL container layout.
 if(count<kEntities){
  Ogre::OldSkeletonInstance* skel=c->getAppearance()->getSkeleton();
  if(skel){
   const unsigned short bones=skel->getNumBones();
   for(unsigned i=0;i<bones && count<kEntities;++i){
    Ogre::OldBone* bone=skel->getBone(static_cast<unsigned short>(i));
    Ogre::TagPoint* tp=bone?dynamic_cast<Ogre::TagPoint*>(bone):NULL;
    if(!tp || tp->getParentEntity()!=body)continue;
    Ogre::MovableObject* o=tp->getChildObject();
    if(!o || o->getMovableType()!="Entity")continue;
    Ogre::Entity* e=static_cast<Ogre::Entity*>(o);bool seen=false;
    for(unsigned k=0;k<count;++k)if(entities[k]==e){seen=true;break;}
    if(!seen)entities[count++]=e;
   }
  }
 }
 return count;
}
unsigned Collect(Character* c,Ogre::Entity** out){__try{return CollectImpl(c,out);}__except(GetExceptionCode()==0xE06D7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return 0;}}
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
 // Materials normally name an Ogre 'unified' wrapper. It has no source file;
 // the already-chosen delegate is the actual native HLSL resource.
 Ogre::HighLevelGpuProgramPtr native=Ogre::HighLevelGpuProgramManager::getSingleton().getByName(key);
 if(native.isNull())throw std::runtime_error("native high-level shader not found: "+key);
 for(unsigned depth=0;depth<8;++depth){
  Ogre::UnifiedHighLevelGpuProgram* unified=dynamic_cast<Ogre::UnifiedHighLevelGpuProgram*>(native.getPointer());
  if(!unified)break;
  native=unified->_getDelegate();
  if(native.isNull())throw std::runtime_error("native shader delegate unavailable: "+key);
 }
 if(native->getSource().empty() && native->getSourceFile().empty())throw std::runtime_error("native shader has no HLSL source: "+key);
 std::string source=native->getSource();
 if(source.empty())source=ReadResource(native->getSourceFile(),native->getGroup());
 source=CharacterMaskSource::Build(source,ReadResource("gbuffer.hlsl",native->getGroup()));
 Ogre::HighLevelGpuProgramPtr p=Program(Unique("MaskFP"),source,Ogre::GPT_FRAGMENT_PROGRAM,native->getParameter("entry_point"),native->getParameter("preprocessor_defines"));
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
 b.hasFarClip=p->getFragmentProgramParameters()->_findNamedConstantDefinition("farClip",false)!=NULL;
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
  if(viewport)a.bounds.merge(Ogre::AxisAlignedBox(e->getWorldAabb().getMinimum(),e->getWorldAabb().getMaximum()));
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
   // The mask depth must share the scene's encoding: radial distance over the
   // live camera far clip, never a stale clone-time or missing auto constant.
   if(b.hasFarClip)p->getFragmentProgramParameters()->setNamedConstant("farClip",camera->getFarClipDistance());
   if(b.colour!=a.colour){p->getFragmentProgramParameters()->setNamedConstant("leopardMaskColour",a.colour);b.colour=a.colour;}
   if(viewport){stage=5;scene->_injectRenderWithPass(p,sub,NULL,false,false);++draws;stage=4;}
  }
 }
 for(unsigned k=0;k<a.bindings.size();)if(a.bindings[k].seen!=frame){Release(a.bindings[k]);a.bindings.erase(a.bindings.begin()+k);}else ++k;
}
void RenderMask(Ogre::Viewport* viewport){
 stage=4;
 for(Actors::iterator i=actors.begin();i!=actors.end();++i){i->second.bounds.setNull();RenderActor(i->second,viewport);}
 callbacksReported=true;
}
void AddBounds(const Ogre::AxisAlignedBox& box,float pad){
 const Ogre::Matrix4 viewProj=camera->getProjectionMatrix()*camera->getViewMatrix(true);
 const Ogre::Vector3 mn=box.getMinimum(),mx=box.getMaximum();
 float x0=Ogre::Math::POS_INFINITY,y0=Ogre::Math::POS_INFINITY,x1=Ogre::Math::NEG_INFINITY,y1=Ogre::Math::NEG_INFINITY;
 for(unsigned i=0;i<8;++i){
  const Ogre::Vector3 c((i&1)?mx.x:mn.x,(i&2)?mx.y:mn.y,(i&4)?mx.z:mn.z);
  const Ogre::Vector4 clip=viewProj*Ogre::Vector4(c.x,c.y,c.z,1.f);
  if(clip.w<=0.01f){ScreenRect full={0,0,static_cast<int>(width),static_cast<int>(height)};rects.push_back(full);return;}
  const float x=(clip.x/clip.w*0.5f+0.5f)*width,y=(0.5f-clip.y/clip.w*0.5f)*height;
  if(x<x0)x0=x;if(x>x1)x1=x;if(y<y0)y0=y;if(y>y1)y1=y;
 }
 const int l=static_cast<int>(std::floor(x0-pad)),t=static_cast<int>(std::floor(y0-pad));
 const int r=static_cast<int>(std::ceil(x1+pad)),b=static_cast<int>(std::ceil(y1+pad));
 ScreenRect rect={l<0?0:l,t<0?0:t,r>static_cast<int>(width)?static_cast<int>(width):r,b>static_cast<int>(height)?static_cast<int>(height):b};
 if(rect.right<=rect.left||rect.bottom<=rect.top)return;
 rects.push_back(rect);
}
void JoinRects(){
 for(bool again=true;again;){again=false;
  for(size_t i=0;i<rects.size() && !again;++i)for(size_t j=i+1;j<rects.size() && !again;++j){
   const ScreenRect&a=rects[i];const ScreenRect&b=rects[j];
   if(a.left>=b.right || b.left>=a.right || a.top>=b.bottom || b.top>=a.bottom)continue;
   ScreenRect merged={(a.left<b.left)?a.left:b.left,(a.top<b.top)?a.top:b.top,(a.right>b.right)?a.right:b.right,(a.bottom>b.bottom)?a.bottom:b.bottom};
   rects[i]=merged;rects.erase(rects.begin()+j);again=true;
  }
 }
}
void UpdateRects(){
 if(rects.empty()){quad->setVisible(false);return;}
 if(rects.size()>8){
  ScreenRect u=rects[0];
  for(size_t i=1;i<rects.size();++i){
   if(rects[i].left<u.left)u.left=rects[i].left;if(rects[i].top<u.top)u.top=rects[i].top;
   if(rects[i].right>u.right)u.right=rects[i].right;if(rects[i].bottom>u.bottom)u.bottom=rects[i].bottom;
  }
  rects.clear();rects.push_back(u);
 }
 float data[32];std::memset(data,0,sizeof(data));
 for(size_t i=0;i<rects.size();++i){
  data[i*4]=static_cast<float>(rects[i].left);data[i*4+1]=static_cast<float>(rects[i].top);
  data[i*4+2]=static_cast<float>(rects[i].right);data[i*4+3]=static_cast<float>(rects[i].bottom);
 }
 Ogre::GpuProgramParametersSharedPtr p=edgeMaterial->getTechnique(0)->getPass(0)->getFragmentProgramParameters();
 const Ogre::GpuConstantDefinition* d=p->_findNamedConstantDefinition("rectList",false);
 if(!d)throw std::runtime_error("edge rect constant missing");
 std::memcpy(p->getFloatPointer(d->physicalIndex),data,sizeof(data));
 p->setNamedConstant("rectCount",static_cast<float>(rects.size()));
 quad->setVisible(true);
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
 DestroySurface();surfaceWorkspace=mainWorkspace;scene=renderer->getSceneManager();camera=renderer->getCamera();width=w;height=h;lastFarClip=-1;
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
 maskTexture=Ogre::TextureManager::getSingleton().createManual(Unique("MaskTexture"),Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,Ogre::TEX_TYPE_2D,width,height,0,Ogre::PF_FLOAT16_RGBA,Ogre::TU_RENDERTARGET);
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
 quad=scene->createManualObject();quad->begin(edgeMaterial->getName(),Ogre::RenderOperation::OT_TRIANGLE_LIST);
 quad->position(-1,1,0);quad->textureCoord(0,0);quad->position(-1,-1,0);quad->textureCoord(0,1);
 quad->position(1,-1,0);quad->textureCoord(1,1);quad->position(1,1,0);quad->textureCoord(1,0);quad->quad(0,1,2,3);quad->end();
 quad->setRenderQueueGroup(88);quad->setLocalAabb(Ogre::Aabb::BOX_INFINITE);
 quadNode=scene->getRootSceneNode()->createChildSceneNode();quadNode->attachObject(quad);quad->setVisible(false);
}
void UpdateMaskImpl(){
 if(failed || actors.empty()){if(quad)quad->setVisible(false);return;}
 if(!ou || !ou->render)return;Renderer* renderer=static_cast<Renderer*>(ou->render);
 stage=1;EnsureSurface(renderer);createsLeft=64;callbacksReported=false;stage=2;
 // All shader compilation/allocation happens before entering workspace update.
 for(Actors::iterator i=actors.begin();i!=actors.end();++i)RenderActor(i->second,NULL);
 stage=3;maskWorkspace->_beginUpdate(false);
 UpdateWorkspace(maskWorkspace);stage=6;
 if(!callbacksReported)throw std::runtime_error("mask scene callback not invoked");
 rects.clear();
 for(Actors::iterator i=actors.begin();i!=actors.end();++i)if(!i->second.bounds.isNull())AddBounds(i->second.bounds,outlineWidth+2.f);
 JoinRects();UpdateRects();
 Ogre::GpuProgramParametersSharedPtr p=edgeMaterial->getTechnique(0)->getPass(0)->getFragmentProgramParameters();
 p->setNamedConstant("radius",outlineWidth);p->setNamedConstant("brightness",outlineBrightness);
 const float farClip=camera->getFarClipDistance();
 if(farClip!=lastFarClip){p->setNamedConstant("invFarClip",farClip>0?1.f/farClip:0.f);lastFarClip=farClip;}
}
// Explicit one-shot development request only. Readback never runs as part of
// normal highlighting, and the request is consumed before any allocation/I/O.
void CaptureMaskIfRequested(){
 if(!debug || !draws || maskTexture.isNull())return;
 static DWORD last=0;DWORD now=GetTickCount();if(now-last<1000)return;last=now;
 const char* request="mods/Character-Highlight/contour-capture.request";
 if(GetFileAttributesA(request)==INVALID_FILE_ATTRIBUTES || !DeleteFileA(request))return;
 Ogre::Image image;maskTexture->convertToImage(image);
 const unsigned w=static_cast<unsigned>(image.getWidth()),h=static_cast<unsigned>(image.getHeight());
 if(w>4096 || h>4096 || static_cast<size_t>(w)*h>8388608){DebugLog("Character Highlight capture skipped: size limit");return;}
 std::vector<float> pixels(w*h*4);Ogre::PixelBox dst(w,h,1,Ogre::PF_FLOAT32_RGBA,&pixels[0]);
 Ogre::PixelUtil::bulkPixelConversion(image.getPixelBox(),dst);
 std::vector<unsigned char> rgb(w*h*3);unsigned nonzero=0,coloured=0;float nearDepth=1,farDepth=0;
 for(unsigned i=0;i<w*h;++i){
  float* p=&pixels[i*4];if(p[3]>0){++nonzero;if(p[3]<nearDepth)nearDepth=p[3];if(p[3]>farDepth)farDepth=p[3];}
  if(p[0]!=0 || p[1]!=0 || p[2]!=0)++coloured;
  for(unsigned j=0;j<3;++j)rgb[i*3+j]=static_cast<unsigned char>((std::max)(0.f,(std::min)(1.f,p[j]))*255);
 }
 Ogre::Image preview;preview.loadDynamicImage(&rgb[0],w,h,1,Ogre::PF_BYTE_RGB,false);
 preview.save("mods/Character-Highlight/contour-mask.png");
 char note[512];sprintf_s(note,"Character Highlight capture: mask=%ux%u alphaPixels=%u rgbPixels=%u depth=%.6f..%.6f draws=%u rects=%u quadVisible=%u queue=%u",w,h,nonzero,coloured,nearDepth,farDepth,draws,static_cast<unsigned>(rects.size()),quad->isVisible()?1:0,quad->getRenderQueueGroup());DebugLog(note);
 unsigned reported=0;
 for(Actors::iterator i=actors.begin();i!=actors.end() && reported<4;++i)for(unsigned j=0;j<i->second.bindings.size() && reported<4;++j){
  Binding& b=i->second.bindings[j];if(b.mask.isNull() || b.original.isNull())continue;
  Ogre::GpuProgramParametersSharedPtr p=b.mask->getTechnique(0)->getPass(0)->getFragmentProgramParameters();
  Ogre::GpuProgramParametersSharedPtr original=b.original->getTechnique(0)->getPass(0)->getFragmentProgramParameters();
  const Ogre::GpuConstantDefinition* d=p->_findNamedConstantDefinition("farClip",false);
  const Ogre::GpuConstantDefinition* o=original->_findNamedConstantDefinition("farClip",false);
  unsigned farAuto=0;if(d){const Ogre::GpuProgramParameters::AutoConstantList& list=p->getAutoConstantList();for(size_t a=0;a<list.size();++a)if(list[a].physicalIndex==d->physicalIndex)farAuto=static_cast<unsigned>(list[a].paramType)+1;}
  sprintf_s(note,"Character Highlight capture params: material=%.80s cameraFar=%.3f maskFar=%.3f nativeFar=%.3f maskAutos=%u nativeAutos=%u farAutoTypePlusOne=%u",b.original->getName().c_str(),camera->getFarClipDistance(),d?*p->getFloatPointer(d->physicalIndex):-1.f,o?*original->getFloatPointer(o->physicalIndex):-1.f,static_cast<unsigned>(p->getAutoConstantCount()),static_cast<unsigned>(original->getAutoConstantCount()),farAuto);DebugLog(note);++reported;
 }
}
bool UpdateNative(){__try{UpdateMaskImpl();return true;}__except(RecordFault(GetExceptionInformation())){return false;}}
void UpdateMask(){
 LARGE_INTEGER begin,end,freq;if(debug)QueryPerformanceCounter(&begin);draws=0;
 try{if(!UpdateNative()){
 HMODULE mod=NULL;char path[MAX_PATH]={0},note[420];
 GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<const char*>(faultAddress),&mod);
 if(mod)GetModuleFileNameA(mod,path,MAX_PATH);const char* name=strrchr(path,'\\');name=name?name+1:path;
 sprintf_s(note,"native fault code=%08lX stage=%u %s+0x%llX access=0x%llX",faultCode,stage,name,reinterpret_cast<ULONG_PTR>(faultAddress)-reinterpret_cast<ULONG_PTR>(mod),faultAccess);LogFailure(note);
 }else CaptureMaskIfRequested();}catch(const std::exception& e){LogFailure(e.what());}catch(...){LogFailure("render failure");}
 if(debug && !actors.empty()){
  QueryPerformanceCounter(&end);QueryPerformanceFrequency(&freq);
  static double total=0,maximum=0;static unsigned samples=0;static DWORD last=0;
  const double ms=(end.QuadPart-begin.QuadPart)*1000.0/freq.QuadPart;total+=ms;if(ms>maximum)maximum=ms;++samples;
  DWORD now=GetTickCount();if(now-last>=10000){
   char note[280];sprintf_s(note,"Character Highlight mask: actors=%u materials=%u programs=%u draws=%u size=%ux%u farClip=%.1f CPUmean=%.3fms max=%.3fms",static_cast<unsigned>(actors.size()),materialCount,static_cast<unsigned>(programs.size()),draws,width,height,camera?camera->getFarClipDistance():0.f,total/samples,maximum);
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
