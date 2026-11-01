"""Compile the actual cleanup functions with VC10 /EHsc and hostile Ogre seams.

No game is launched. The doubles stand in for native objects that can disappear,
shrink, or receive another mod's material between highlight and key release.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

SOURCE = Path(__file__).resolve().parents[1] / 'Container-Highlight.cpp'
BASELINE = 'adc8b08a660396cd2e45040234e52be37c06eddc'


def function(source, name):
    # Find the beginning of the definition rather than a preceding declaration.
    import re
    found = re.search(r'^(?:bool|void|PhysicsCollection\*|Ogre::SceneNode\*) ' + name + r'\([^;]*?\)\s*\{', source, re.M)
    if not found:
        raise ValueError('Missing definition: ' + name)
    start = found.start()
    brace = source.index('{', found.start())
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


PRELUDE = r'''
#define NOMINMAX
#include <Windows.h>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <string>
static int releases=0, errors=0, touches=0, escaped=0, failed=0, collections=0, checks=0, collectorFault=0;
namespace Ogre {
struct MovableObject {std::string type;MovableObject(const char* t):type(t){}const std::string& getMovableType(){if(collectorFault==6)throw std::runtime_error("movable type fault");return type;}};
struct EntityFactory {static const std::string FACTORY_TYPE_NAME;};
const std::string EntityFactory::FACTORY_TYPE_NAME="Entity";
struct Material { int id; Material(int v):id(v){} };
struct MaterialPtr {
    Material* p; MaterialPtr(Material* v=0):p(v){}
    bool isNull()const{return p==0;} void setNull(){p=0;}
    Material* getPointer()const{return p;}
    Material* operator->()const{return p;}
};
struct SubEntity {
    MaterialPtr value; int fault; int writes;
    SubEntity():fault(0),writes(0){}
    const MaterialPtr& getMaterial()const{return value;}
    void setMaterial(const MaterialPtr& v){++touches; if(fault==1) RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,0); if(fault==2)throw std::runtime_error("native Ogre failure"); value=v;++writes;}
};
struct Entity:MovableObject {
    std::vector<SubEntity> parts; int fault; int outOfRange;
    Entity():MovableObject("Entity"),fault(0),outOfRange(0){}
    size_t getNumSubEntities(){++touches; if(fault)RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,0);return parts.size();}
    SubEntity* getSubEntity(size_t n){++touches;if(fault)RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,0);if(n>=parts.size()){++outOfRange;throw std::out_of_range("Ogre checked index");}return &parts[n];}
};
}
namespace Ogre {
struct InstanceBatch:MovableObject {MaterialPtr value;int fault;InstanceBatch():MovableObject("InstanceBatch"),fault(0){}const MaterialPtr& getMaterial(){++touches;if(fault==1)RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,0);if(fault==2)throw std::runtime_error("batch fault");return value;}};
struct InstancedEntity:MovableObject {InstanceBatch* owner;InstancedEntity():MovableObject("InstancedEntity"),owner(0){}InstanceBatch* _getOwner(){return owner;}};
struct SceneNode {size_t numAttachedObjects();MovableObject* getAttachedObject(size_t);size_t numChildren(){if(collectorFault==3)RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,0);return 0;}SceneNode* getChild(size_t){return 0;}};
}
struct TintEntityBinding {
    Ogre::Entity* entity; std::vector<Ogre::MaterialPtr> originalMaterials,cloneMaterials; bool hasApplied;
    TintEntityBinding():entity(0),hasApplied(false){}
};
template<class T> struct lektor:std::vector<T> {};
struct PhysicsCollection {struct StaticEnt {Ogre::MovableObject* ent;};lektor<StaticEnt*> staticEnts;};
struct Building {PhysicsCollection* physical;Building():physical(0){}bool isValid(){if(collectorFault==5)RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,0);return true;}Ogre::SceneNode* getRootNode(){if(collectorFault==4)RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,0);static Ogre::SceneNode node;return &node;}};
struct hand {int value;hand(int v=0):value(v){}Building* getBuilding()const{++collections;if(collectorFault==1)RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,0);if(collectorFault==2)throw std::runtime_error("collector fault");static Building building;building.physical=collectorFault==7?reinterpret_cast<PhysicsCollection*>(1):0;return &building;}};
struct TintBatchBinding {Ogre::InstanceBatch* batch;Ogre::MaterialPtr originalMaterial,cloneMaterial;TintBatchBinding():batch(0){}};
struct TintOverlayBinding {};
struct ContainerTintEntry { hand targetHandle;std::vector<TintEntityBinding> entityBindings;std::vector<TintBatchBinding> batchBindings;TintOverlayBinding overlayBinding; };
struct RuntimeState {std::vector<ContainerTintEntry> tintEntries;} g_state;
static std::vector<Ogre::Entity*> currentEntities;
static std::vector<Ogre::InstanceBatch*> currentBatches;
const bool kEnableInstancedBatchTint=true; // Exercise disabled batch code offline only.
bool IsHighlightableContainerBuilding(Building*){return true;}
size_t Ogre::SceneNode::numAttachedObjects(){return currentEntities.size()+currentBatches.size();}
Ogre::MovableObject* Ogre::SceneNode::getAttachedObject(size_t n){if(n<currentEntities.size())return currentEntities[n];return currentBatches[n-currentEntities.size()];}
void CollectTintTargetsForTarget(const hand&,std::vector<Ogre::Entity*>*,std::vector<Ogre::InstanceBatch*>*);
void ClearTintOverlayBinding(TintOverlayBinding*){}
void ReleaseTintClone(Ogre::MaterialPtr& m){if(!m.isNull())++releases;m.setNull();}
void LogErrorLine(const char*){++errors;}
bool EntityVectorContains(const std::vector<Ogre::Entity*>& v,Ogre::Entity* p){return std::find(v.begin(),v.end(),p)!=v.end();}
bool InstanceBatchVectorContains(const std::vector<Ogre::InstanceBatch*>& v,Ogre::InstanceBatch* p){return std::find(v.begin(),v.end(),p)!=v.end();}
void AppendTintEntityUnique(std::vector<Ogre::Entity*>* out,Ogre::Entity* entity){if(!EntityVectorContains(*out,entity))out->push_back(entity);}
void AppendTintBatchUnique(std::vector<Ogre::InstanceBatch*>* out,Ogre::InstanceBatch* batch){if(!InstanceBatchVectorContains(*out,batch))out->push_back(batch);}
'''

TESTS = r'''
void check(bool ok,const char* message){++checks;if(!ok){++failed;printf("FAIL: %s\n",message);}}
void runClear(TintEntityBinding* b,const std::vector<Ogre::Entity*>* live){
    __try { CLEAR_CALL; } __except(EXCEPTION_EXECUTE_HANDLER){++escaped;}
}
void runClearAll(){__try {ClearAllTint();}__except(EXCEPTION_EXECUTE_HANDLER){++escaped;}}
void runClearBatch(TintBatchBinding* b,const std::vector<Ogre::InstanceBatch*>* live){__try{BATCH_CALL;}__except(EXCEPTION_EXECUTE_HANDLER){++escaped;}}
void setup(TintEntityBinding& b,Ogre::Entity& e,Ogre::Material* original,Ogre::Material* clone,size_t n){
    b.entity=&e;b.hasApplied=true;b.originalMaterials.assign(n,Ogre::MaterialPtr(original));b.cloneMaterials.assign(n,Ogre::MaterialPtr(clone));
    e.parts.resize(n);for(size_t i=0;i<n;++i)e.parts[i].value=Ogre::MaterialPtr(clone);
}
int main(){
    Ogre::Material original(1),clone(2),foreign(3);
    { Ogre::Entity e;TintEntityBinding b;setup(b,e,&original,&clone,1);std::vector<Ogre::Entity*> live(1,&e);runClear(&b,&live);
      check(e.parts[0].value.p==&original,"ordinary release restores original");check(!b.entity&&b.originalMaterials.empty()&&b.cloneMaterials.empty(),"ordinary release clears cache");}
    { Ogre::Entity e;TintEntityBinding b;setup(b,e,&original,&clone,1);e.parts[0].value=Ogre::MaterialPtr(&foreign);std::vector<Ogre::Entity*> live(1,&e);runClear(&b,&live);
      check(e.parts[0].value.p==&foreign,"external material survives release");}
    { Ogre::Entity e;TintEntityBinding b;setup(b,e,&original,&clone,3);e.parts.resize(1);std::vector<Ogre::Entity*> live(1,&e);int before=escaped;runClear(&b,&live);
      check(escaped==before&&e.outOfRange==0,"rebuilt smaller mesh never indexes removed subentity");check(!b.entity&&b.cloneMaterials.empty(),"smaller mesh is retired");}
    { Ogre::Entity e;TintEntityBinding b;setup(b,e,&original,&clone,1);e.fault=1;std::vector<Ogre::Entity*> live;int before=touches;runClear(&b,&live);
      check(touches==before,"detached entity never dereferenced");check(!b.entity&&b.cloneMaterials.empty(),"detached cache retired");}
    { Ogre::Entity e;TintEntityBinding b;setup(b,e,&original,&clone,1);e.parts[0].fault=1;std::vector<Ogre::Entity*> live(1,&e);int before=escaped;runClear(&b,&live);for(int i=0;i<100;++i)runClear(&b,&live);
      check(escaped==before,"hardware exception cannot abort caller or repeat every frame");check(!b.entity&&b.cloneMaterials.empty(),"faulting cache retired despite native exception");}
    { Ogre::Entity e;TintEntityBinding b;setup(b,e,&original,&clone,2);e.parts[0].fault=2;std::vector<Ogre::Entity*> live(1,&e);runClear(&b,&live);
      check(!b.entity&&b.cloneMaterials.empty(),"C++ native exception retires cache");check(e.parts[1].writes==0,"no further native touches after C++ restore fault");}
    { Ogre::Entity e;TintEntityBinding b;setup(b,e,&original,&clone,2);e.parts[0].fault=1;std::vector<Ogre::Entity*> live(1,&e);runClear(&b,&live);check(e.parts[1].writes==0,"no further native touches after SEH restore fault");}
    { Ogre::Entity e;TintEntityBinding b;setup(b,e,&original,&clone,1);int before=touches;runClear(&b,0);check(touches==before&&!b.entity,"unvalidated default never touches cached entity");}
    { Ogre::Entity e;TintEntityBinding b;setup(b,e,&original,&clone,1);e.fault=1;std::vector<Ogre::Entity*> live(1,&e);int before=escaped;runClear(&b,&live);
      check(escaped==before&&!b.entity&&b.cloneMaterials.empty(),"count read fault is contained and binding retired");}
    { Ogre::Entity e;TintEntityBinding b;setup(b,e,&original,&clone,2);b.cloneMaterials.resize(1);std::vector<Ogre::Entity*> live(1,&e);runClear(&b,&live);
      check(e.parts[0].value.p==&original&&e.parts[1].value.p==&clone,"unpaired material index is never restored");}
    { int before=touches;runClear(0,0);check(touches==before,"null binding is harmless");}
    { Ogre::Entity e;TintEntityBinding b;setup(b,e,&original,&clone,1);ContainerTintEntry entry;entry.targetHandle=17;entry.entityBindings.push_back(b);g_state.tintEntries.push_back(entry);currentEntities.assign(1,&e);runClearAll();
      check(g_state.tintEntries.empty()&&e.parts[0].value.p==&original,"full cleanup resolves current model then restores and erases entry");int before=collections;for(int i=0;i<100;++i)runClearAll();check(collections==before,"inactive frames perform no model collection");g_state.tintEntries.clear();}
    { Ogre::Entity e;TintEntityBinding b;setup(b,e,&original,&clone,1);e.fault=1;ContainerTintEntry entry;entry.targetHandle=17;entry.entityBindings.push_back(b);g_state.tintEntries.push_back(entry);currentEntities.clear();int before=touches;runClearAll();
      check(g_state.tintEntries.empty()&&touches==before,"full cleanup drops detached model without dereference");g_state.tintEntries.clear();}
    for(int fault=1;fault<=7;++fault){Ogre::Entity e;TintEntityBinding b;setup(b,e,&original,&clone,1);ContainerTintEntry entry;entry.targetHandle=17;entry.entityBindings.push_back(b);g_state.tintEntries.push_back(entry);currentEntities.assign(1,&e);collectorFault=fault;int before=escaped,errorBefore=errors;runClearAll();
      check(escaped==before&&g_state.tintEntries.empty(),"collector fault retires full entry without escaping");check(e.parts[0].writes==0,"partial collector results cannot authorize restore");check(errors>errorBefore,"collector faults produce an always-visible error");g_state.tintEntries.clear();collectorFault=0;}
    {Ogre::InstanceBatch batch;batch.value=Ogre::MaterialPtr(&clone);TintBatchBinding b;b.batch=&batch;b.originalMaterial=Ogre::MaterialPtr(&original);b.cloneMaterial=Ogre::MaterialPtr(&clone);std::vector<Ogre::InstanceBatch*> live(1,&batch);runClearBatch(&b,&live);check(batch.value.p==&original&&!b.batch&&b.cloneMaterial.isNull(),"live batch restores owned clone and retires");}
    {Ogre::InstanceBatch batch;batch.value=Ogre::MaterialPtr(&foreign);TintBatchBinding b;b.batch=&batch;b.originalMaterial=Ogre::MaterialPtr(&original);b.cloneMaterial=Ogre::MaterialPtr(&clone);std::vector<Ogre::InstanceBatch*> live(1,&batch);runClearBatch(&b,&live);check(batch.value.p==&foreign,"batch external material survives");}
    for(int fault=1;fault<=2;++fault){Ogre::InstanceBatch batch;batch.value=Ogre::MaterialPtr(&clone);batch.fault=fault;TintBatchBinding b;b.batch=&batch;b.originalMaterial=Ogre::MaterialPtr(&original);b.cloneMaterial=Ogre::MaterialPtr(&clone);std::vector<Ogre::InstanceBatch*> live(1,&batch);int before=escaped;runClearBatch(&b,&live);check(escaped==before&&!b.batch&&b.cloneMaterial.isNull(),"batch native fault contained and cache retired");}
    {Ogre::InstanceBatch batch;batch.fault=1;TintBatchBinding b;b.batch=&batch;b.originalMaterial=Ogre::MaterialPtr(&original);b.cloneMaterial=Ogre::MaterialPtr(&clone);std::vector<Ogre::InstanceBatch*> live;int before=touches;runClearBatch(&b,&live);check(touches==before&&!b.batch,"detached batch is never touched");}
    {Ogre::InstanceBatch batch;batch.value=Ogre::MaterialPtr(&clone);TintBatchBinding b;b.batch=&batch;b.originalMaterial=Ogre::MaterialPtr(&original);b.cloneMaterial=Ogre::MaterialPtr(&clone);ContainerTintEntry entry;entry.targetHandle=17;entry.batchBindings.push_back(b);g_state.tintEntries.push_back(entry);currentEntities.clear();currentBatches.assign(1,&batch);runClearAll();check(g_state.tintEntries.empty()&&batch.value.p==&original,"batch-only entry resolves current batch before restore");currentBatches.clear();}
    check(escaped==0,"no native exception escapes cleanup");check(errors>=2,"restore errors visible with detailed logging disabled");
    printf("restore lifetime: %d checks, %d failures, %d escaped native faults, %d clone releases\n",checks,failed,escaped,releases);return failed?1:0;
}
'''


def main():
    source = SOURCE.read_text(encoding='utf-8-sig')
    if '--baseline' in sys.argv:
        source = subprocess.check_output(['git', 'show', BASELINE + ':Container-Highlight/Container-Highlight.cpp'], cwd=SOURCE.parent, text=True, encoding='utf-8')
    definitions = function(source, 'MaterialPtrsReferSameObject') + '\n' + function(source, 'ReleaseTintClones')
    for name in ('AppendMovableAsTintEntityUnique', 'AppendMovableAsTintBatchUnique',
                 'TryGetBuildingPhysical', 'TryGetBuildingRootNode',
                 'CollectBuildingEntitiesRecursive', 'CollectBuildingInstanceBatchesRecursive',
                 'CollectBuildingPhysicalEntities', 'CollectBuildingPhysicalInstanceBatches'):
        definitions += '\n' + function(source, name)
    for name in ('TryRestoreTintEntityMaterial', 'TryGetTintEntityPartCount', 'TryRestoreTintBatchMaterial', 'TryCollectTintRestoreTargets'):
        if ('bool ' + name + '(') in source:
            definitions += '\n' + function(source, name)
    definitions += '\n' + function(source, 'ClearTintBinding')
    definitions += '\n' + function(source, 'SetInstanceBatchMaterial')
    definitions += '\n' + function(source, 'ClearTintBatchBinding')
    definitions += '\n' + function(source, 'CollectTintTargetsForTarget')
    definitions += '\n' + function(source, 'ClearTintEntry')
    definitions += '\n' + function(source, 'ClearAllTint')
    call = 'ClearTintBinding(b,live)' if 'const std::vector<Ogre::Entity*>* liveEntities' in definitions else 'ClearTintBinding(b)'
    batch_call = 'ClearTintBatchBinding(b,live)' if 'const std::vector<Ogre::InstanceBatch*>* liveBatches' in definitions else 'ClearTintBatchBinding(b)'
    vc = Path(r'C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC')
    sdk = Path(r'C:\Program Files\Microsoft SDKs\Windows\v7.1')
    env = os.environ.copy()
    env['INCLUDE'] = str(vc / 'include') + ';' + str(sdk / 'Include')
    env['LIB'] = str(vc / 'lib/amd64') + ';' + str(sdk / 'Lib/x64')
    env['PATH'] = str(vc / 'bin/amd64') + ';' + env['PATH']
    with tempfile.TemporaryDirectory(prefix='container-restore-') as temp:
        cpp = Path(temp) / 'restore.cpp'
        collector_decl = function(source, 'CollectTintTargetsForTarget').split('{', 1)[0] + ';'
        prelude = PRELUDE.replace('void CollectTintTargetsForTarget(const hand&,std::vector<Ogre::Entity*>*,std::vector<Ogre::InstanceBatch*>*);', collector_decl)
        cpp.write_text(prelude + definitions + TESTS.replace('CLEAR_CALL', call).replace('BATCH_CALL', batch_call), encoding='utf-8')
        subprocess.run([str(vc / 'bin/amd64/cl.exe'), '/nologo', '/EHsc', '/MD', '/O2', str(cpp), '/link', 'kernel32.lib'], cwd=temp, env=env, check=True)
        return subprocess.run([str(Path(temp) / 'restore.exe')], cwd=temp, env=env).returncode


if __name__ == '__main__':
    raise SystemExit(main())
