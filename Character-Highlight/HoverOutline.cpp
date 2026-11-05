#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include "HoverOutline.h"
#include "HoverOutlineShaders.h"
#include <kenshi/Appearance.h>
#include <kenshi/Character.h>
#include <kenshi/util/hand.h>
#include <ogre/OgreEntity.h>
#include <ogre/OgreSubEntity.h>
#include <ogre/OgreSceneNode.h>
#include <ogre/OgreHighLevelGpuProgramManager.h>
#include <ogre/OgreHighLevelGpuProgram.h>
#include <ogre/OgreResourceGroupManager.h>
#include <ogre/OgreMaterialManager.h>
#include <ogre/OgreTechnique.h>
#include <ogre/OgrePass.h>
#include <Debug.h>
#include <map>
#include <vector>
#include <sstream>
#include <stdexcept>

namespace HoverOutline {
namespace {
const unsigned kActorLimit=512, kMaterialLimit=4096, kEntityLimit=64, kSubLimit=128;
const char* const kPassName="LEOPARD_CharacterContour";
unsigned frame=0, names=0, materialCount=0, createsLeft=0;
bool failed=false, limitReported=false;
bool cleanupReported=false;
void CleanupRefused() {
    failed=true;
    if(!cleanupReported) {cleanupReported=true;ErrorLog("Character Highlight: contour disabled after scene/material cleanup refusal");}
}
struct Key {
    unsigned v[5];
    explicit Key(const hand& h) {v[0]=h.type;v[1]=h.container;v[2]=h.containerSerial;v[3]=h.index;v[4]=h.serial;}
    bool operator<(const Key& b) const {
        for(unsigned i=0;i<5;++i)if(v[i]!=b.v[i])return v[i]<b.v[i];return false;
    }
};
struct Binding {
    Ogre::SubEntity* address; // Comparison only, never dereferenced across frames.
    Ogre::MaterialPtr original, copy;
    unsigned seen;
    Ogre::ColourValue colour;
    float width;
    bool suspended;
    bool unsupported;
    Binding():address(NULL),seen(0),colour(-1.f,-1.f,-1.f,-1.f),width(-1),suspended(false),unsupported(false) {}
};
struct Actor {hand who; unsigned seen; std::vector<Binding> bindings; Actor():seen(0) {}};
typedef std::map<Key,Actor> Actors;
Actors actors;
hand portraitStack[16];
unsigned portraitDepth=0;
Ogre::Entity* Body(Character* c) {
    __try {return c && c->getAppearance()?c->getAppearance()->body:NULL;}
    __except(EXCEPTION_EXECUTE_HANDLER) {return NULL;}
}
// Equipment is reached through the appearance's public scene-node tree, without
// reading undocumented AttachedEntity fields or walking the whole world.
unsigned CollectImpl(Character* c,Ogre::Entity** entities) {
    Ogre::Entity* body=Body(c); if(!body)return 0;
    unsigned count=1;entities[0]=body;
    Ogre::Node* nodes[128];unsigned nodeCount=1;nodes[0]=c->getAppearance()->getSceneNode();
    for(unsigned i=0;i<nodeCount && count<kEntityLimit;++i) {
        Ogre::Node* node=nodes[i];if(!node)continue;
        Ogre::SceneNode* scene=dynamic_cast<Ogre::SceneNode*>(node);
        if(scene)for(size_t j=0;j<scene->numAttachedObjects() && j<kEntityLimit;++j) {
            Ogre::MovableObject* child=scene->getAttachedObject(j);
            if(!child || child->getMovableType()!="Entity")continue;
            Ogre::Entity* e=static_cast<Ogre::Entity*>(child);bool seen=false;
            for(unsigned k=0;k<count;++k)if(entities[k]==e) {seen=true;break;}
            if(!seen && count<kEntityLimit)entities[count++]=e;
        }
        for(size_t j=0;j<node->numChildren() && nodeCount<128;++j) {
            Ogre::Node* child=node->getChild(j);bool seen=false;
            for(unsigned k=0;k<nodeCount;++k)if(nodes[k]==child) {seen=true;break;}
            if(!seen)nodes[nodeCount++]=child;
        }
    }
    return count;
}
unsigned Collect(Character* c,Ogre::Entity** entities) {
    __try {return CollectImpl(c,entities);}
    __except(EXCEPTION_EXECUTE_HANDLER) {return 0;}
}
Ogre::Pass* Contour(Binding& b) {
    if(b.copy.isNull() || !b.copy->getNumTechniques())return NULL;
    Ogre::Technique* t=b.copy->getTechnique(0);
    for(unsigned short i=0;i<t->getNumPasses();++i)if(t->getPass(i)->getName()==kPassName)return t->getPass(i);
    return NULL;
}
void ReleaseImpl(Binding& b) {
    if(!b.copy.isNull()) {
        Ogre::Pass* p=Contour(b);if(p)p->getFragmentProgramParameters()->setNamedConstant("outlineEnabled",0.f);
        Ogre::MaterialManager::getSingleton().remove(b.copy->getHandle());
        b.copy.setNull();if(materialCount)--materialCount;
    }
    b.original.setNull();
    b.width=-1.f;b.colour=Ogre::ColourValue(-1.f,-1.f,-1.f,-1.f);b.suspended=false;b.unsupported=false;
}
// Separate guarded calls from C++ owners for VC10 SEH/unwinding compatibility.
bool ReleaseNative(Binding* b) {
    __try {ReleaseImpl(*b);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
void Release(Binding& b) {
    try {if(!ReleaseNative(&b))CleanupRefused();}
    catch(...) {CleanupRefused();}
}
void RestoreNativeImpl(Actor& a) {
    Ogre::Entity* entities[kEntityLimit];unsigned n=Collect(a.who.getCharacter(),entities);
    for(unsigned i=0;i<n;++i)for(unsigned j=0;j<entities[i]->getNumSubEntities() && j<kSubLimit;++j) {
        Ogre::SubEntity* sub=entities[i]->getSubEntity(j);
        for(size_t k=0;k<a.bindings.size();++k) {
            Binding& b=a.bindings[k];
            if(sub==b.address && sub->getMaterial()==b.copy) {sub->setMaterial(b.original);break;}
        }
    }
}
bool RestoreNative(Actor* a) {
    __try {RestoreNativeImpl(*a);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
void Restore(Actor& a) {
    try {if(!RestoreNative(&a))CleanupRefused();}catch(...) {CleanupRefused();}
    for(size_t k=0;k<a.bindings.size();++k)Release(a.bindings[k]);
    a.bindings.clear();
}
Ogre::HighLevelGpuProgramPtr Program(const std::string& name,const char* source,Ogre::GpuProgramType type,const std::string& defines) {
    Ogre::HighLevelGpuProgramManager& m=Ogre::HighLevelGpuProgramManager::getSingleton();
    Ogre::HighLevelGpuProgramPtr p=m.getByName(name);if(!p.isNull())return p;
    p=m.createProgram(name,Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,"hlsl",type);
    p->setSource(source);p->setParameter("entry_point",type==Ogre::GPT_VERTEX_PROGRAM?"main_vs":"main_fs");
    p->setParameter("target",type==Ogre::GPT_VERTEX_PROGRAM?"vs_4_0":"ps_4_0");
    p->setParameter("column_major_matrices","false");
    if(!defines.empty())p->setParameter("preprocessor_defines",defines);
    if(type==Ogre::GPT_VERTEX_PROGRAM)p->setSkeletalAnimationIncluded(defines.find("SKINNED")!=std::string::npos);
    p->load();if(p->hasCompileError())throw std::runtime_error("contour shader compilation failed");
    return p;
}
void Attach(Binding& b,Ogre::SubEntity* sub,Ogre::Entity* entity) {
    b.address=sub;b.original=sub->getMaterial();
    b.unsupported=true;
    if(b.original.isNull() || !b.original->getNumTechniques())return;
    Ogre::Technique* original=b.original->getTechnique(0);
    if(!original->getNumPasses() || !original->getPass(0)->hasVertexProgram())return;
    Ogre::Pass* base=original->getPass(0);
    // Hair cards need a dedicated alpha-mask variant; do not draw a rectangle
    // around them. Opaque clothing, hats and weapons use the normal contour.
    if(base->hasFragmentProgram() && base->getFragmentProgramName().find("Hair")!=std::string::npos)return;
    Ogre::GpuProgramParametersSharedPtr source=base->getVertexProgramParameters();
    bool mask=source->_findNamedConstantDefinition("hiddenMask",false)!=NULL;
    bool collapse=source->_findNamedConstantDefinition("hideSide",false)!=NULL;
    bool skin=entity->hasSkeleton() && source->_findNamedConstantDefinition("worldMatrix3x4Array",false)!=NULL;
    unsigned variant=(mask?1:0)|(collapse?2:0)|(skin?4:0);
    std::ostringstream vpName;vpName<<"LEOPARD_Contour_VP"<<variant;
    std::string defines=skin?"SKINNED":"";
    if(mask)defines+=(defines.empty()?"":",")+std::string("MASKED");
    if(collapse)defines+=(defines.empty()?"":",")+std::string("COLLAPSE_SIDE");
    Program(vpName.str(),HoverOutlineShaders::vertex,Ogre::GPT_VERTEX_PROGRAM,defines);
    Program("LEOPARD_Contour_FP",HoverOutlineShaders::fragment,Ogre::GPT_FRAGMENT_PROGRAM,"");
    std::ostringstream name;name<<"LEOPARD_ContourMaterial_"<<++names;
    b.copy=b.original->clone(name.str());++materialCount;
    Ogre::Technique* t=b.copy->getTechnique(0);
    // Preserve the original parameter objects: native limb/clothing updates and
    // Vital Sense keep updating them, with no per-frame full-buffer copy.
    for(unsigned short i=0;i<original->getNumPasses();++i) {
        Ogre::Pass* from=original->getPass(i);Ogre::Pass* to=t->getPass(i);
        if(from->hasVertexProgram())to->setVertexProgramParameters(from->getVertexProgramParameters());
        if(from->hasFragmentProgram())to->setFragmentProgramParameters(from->getFragmentProgramParameters());
    }
    Ogre::Pass* pass=t->createPass();pass->setName(kPassName);
    pass->setVertexProgram(vpName.str());pass->setFragmentProgram("LEOPARD_Contour_FP");
    pass->setCullingMode(base->getCullingMode()==Ogre::CULL_ANTICLOCKWISE?Ogre::CULL_CLOCKWISE:Ogre::CULL_ANTICLOCKWISE);
    pass->setDepthCheckEnabled(true);pass->setDepthWriteEnabled(false);
    Ogre::GpuProgramParametersSharedPtr vp=pass->getVertexProgramParameters();
    if(skin)vp->setNamedAutoConstant("bones",Ogre::GpuProgramParameters::ACT_WORLD_MATRIX_ARRAY_3x4);
    else vp->setNamedAutoConstant("worldMatrix",Ogre::GpuProgramParameters::ACT_WORLD_MATRIX);
    vp->setNamedAutoConstant("viewProjection",Ogre::GpuProgramParameters::ACT_VIEWPROJ_MATRIX);
    vp->setNamedAutoConstant("viewport",Ogre::GpuProgramParameters::ACT_VIEWPORT_SIZE);
    Ogre::GpuProgramParametersSharedPtr fp=pass->getFragmentProgramParameters();
    fp->setNamedConstant("outlineEnabled",1.f);
    fp->setNamedAutoConstant("cameraPos",Ogre::GpuProgramParameters::ACT_CAMERA_POSITION);
    fp->setNamedAutoConstant("farClip",Ogre::GpuProgramParameters::ACT_FAR_CLIP_DISTANCE);
    sub->setMaterial(b.copy);
    b.unsupported=false;
}
}
void BeginFrame() {createsLeft=24;if(++frame==0) {Clear();frame=1;}}
void DrawImpl(Character* c,const Ogre::ColourValue& colour,float width,float brightness) {
    if(!c || failed || colour.a<=0 || width<=0 || brightness<=0)return;
    try {
        Key key(c->handle);Actors::iterator found=actors.find(key);
        if(found==actors.end()) {
            if(actors.size()>=kActorLimit) {if(!limitReported) {ErrorLog("Character Highlight: contour actor limit reached");limitReported=true;}return;}
            found=actors.insert(std::make_pair(key,Actor())).first;found->second.who=c->handle;
        }
        Actor& a=found->second;a.seen=frame;
        Ogre::Entity* entities[kEntityLimit];unsigned count=Collect(c,entities);
        Ogre::ColourValue effective=colour;effective.r*=brightness;effective.g*=brightness;effective.b*=brightness;effective.a=1.f;
        for(unsigned i=0;i<count;++i)for(unsigned j=0;j<entities[i]->getNumSubEntities() && j<kSubLimit;++j) {
            Ogre::SubEntity* sub=entities[i]->getSubEntity(j);size_t k=0;
            for(;k<a.bindings.size();++k)if(a.bindings[k].address==sub)break;
            if(k==a.bindings.size()) {
                if(a.bindings.size()>=kSubLimit || materialCount>=kMaterialLimit)continue;
                a.bindings.push_back(Binding());a.bindings.back().address=sub;
            }
            Binding& b=a.bindings[k];b.seen=frame;
            if(!b.copy.isNull() && sub->getMaterial()!=b.copy)Release(b);
            if(b.copy.isNull()) {
                if(b.unsupported && sub->getMaterial()==b.original)continue;
                if(!createsLeft || materialCount>=kMaterialLimit)continue;
                --createsLeft;Attach(b,sub,entities[i]);
            }
            Ogre::Pass* pass=Contour(b);if(!pass)continue;
            pass->getVertexProgramParameters()->copyMatchingNamedConstantsFrom(*b.original->getTechnique(0)->getPass(0)->getVertexProgramParameters());
            if(b.width!=width) {pass->getVertexProgramParameters()->setNamedConstant("outlinePixels",width);b.width=width;}
            if(b.colour!=effective) {pass->getFragmentProgramParameters()->setNamedConstant("outlineColour",effective);b.colour=effective;}
        }
        for(size_t k=0;k<a.bindings.size();) {
            if(a.bindings[k].seen!=frame) {Release(a.bindings[k]);a.bindings.erase(a.bindings.begin()+k);}
            else ++k;
        }
    } catch(const std::exception& e) {failed=true;Clear();ErrorLog(std::string("Character Highlight contour: ")+e.what());}
    catch(...) {failed=true;Clear();ErrorLog("Character Highlight contour: material/program setup failed");}
}
bool DrawNative(Character* c,const Ogre::ColourValue* colour,float width,float brightness) {
    __try {DrawImpl(c,*colour,width,brightness);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
void Draw(Character* c,const Ogre::ColourValue& colour,float width,float brightness) {
    if(failed)return;
    if(!DrawNative(c,&colour,width,brightness)) {
        failed=true;Clear();ErrorLog("Character Highlight: contour disabled after invalid scene access");
    }
}
void EndFrame() {
    for(Actors::iterator i=actors.begin();i!=actors.end();) {
        if(i->second.seen!=frame) {Restore(i->second);actors.erase(i++);}else ++i;
    }
}
void Remove(Character* c) {
    if(!c)return;Actors::iterator i=actors.find(Key(c->handle));
    if(i!=actors.end()) {Restore(i->second);actors.erase(i);}
}
void Clear() {
    for(Actors::iterator i=actors.begin();i!=actors.end();++i)Restore(i->second);
    actors.clear();materialCount=0;
}
void PortraitBegin(Character* c) {
    if(portraitDepth>=16) {++portraitDepth;return;}
    portraitStack[portraitDepth].setNull();
    if(c)portraitStack[portraitDepth]=c->handle;
    ++portraitDepth;
    if(!c)return;Actors::iterator i=actors.find(Key(c->handle));if(i==actors.end())return;
    for(size_t k=0;k<i->second.bindings.size();++k) {
        Binding& b=i->second.bindings[k];Ogre::Pass* p=Contour(b);
        if(p) {p->getFragmentProgramParameters()->setNamedConstant("outlineEnabled",0.f);b.suspended=true;}
    }
}
void PortraitEnd() {
    if(!portraitDepth)return;
    if(--portraitDepth>=16)return;
    const hand& h=portraitStack[portraitDepth];
    // Nested renders of the same character must keep the outer contour hidden.
    for(unsigned j=0;j<portraitDepth;++j) {
        const hand& outer=portraitStack[j];
        if(outer.type==h.type && outer.container==h.container && outer.containerSerial==h.containerSerial && outer.index==h.index && outer.serial==h.serial)return;
    }
    Actors::iterator i=actors.find(Key(h));if(i==actors.end())return;
    for(size_t k=0;k<i->second.bindings.size();++k) {
            Binding& b=i->second.bindings[k];if(!b.suspended)continue;
            Ogre::Pass* p=Contour(b);if(p)p->getFragmentProgramParameters()->setNamedConstant("outlineEnabled",1.f);b.suspended=false;
    }
}
}
