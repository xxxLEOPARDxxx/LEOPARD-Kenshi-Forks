"""Actual production submit/request/queue bridge and policy against owned native boundaries.
Legacy RED is frozen6e15b3a; no game code is called. Fixture queue observer runs
before the native stub returns, and may retire its owned page at publication.
"""
from pathlib import Path
import os,subprocess,re,struct,importlib.util,sys
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/path-priority-tests';OUT.mkdir(exist_ok=True)
RESEARCH=ROOT/'leopard/fixtures/priority_publication_6e15'
s=(ROOT/'src/pathfind/pathfind_hooks.cpp').read_text(encoding='utf-8')
def body(marker):
 a=s.index(marker);b=s.index('{',a);e=b+1;d=1
 while d:d+=(s[e]=='{')-(s[e]=='}');e+=1
 return s[a:e]
code=(RESEARCH/'publication_fixture.cpp').read_text(encoding='utf-8')
legacy=(RESEARCH/'current-submit.extract.cpp').read_text(encoding='utf-8').strip();assert legacy in code
code=code.replace(legacy,legacy.replace('hook_pathReqSubmit','LegacySubmit'))
code=code.replace('#include <stdexcept>','#include <stdexcept>\n#include "pathfind/path_priority_publication.h"\n#include "pathfind/player_repath_tier_policy.h"\n#include "planner/planner_water_table.h"\n#include "planner/plan_policy.h"')
code=code.replace('namespace planner{void PlannerWaterOnSubmit(float*){}}','')
code=code.replace('volatile LONG playerRequestsInFlight;','volatile LONG playerRequestsInFlight,diagPlayerRequests,diagNPCRequests;').replace('Diag g_pathDiag={};}','Diag g_pathDiag={};struct Cfg{bool playerRepathTierEnabled;};Cfg g_pathfindCfg={true};}')
code=code.replace('void(*orig_pathReqSubmit)(void*,void*,bool);','void(*orig_pathReqSubmit)(void*,void*,bool);void(*orig_requestPath)(void*,float*,int);')
a=code.index('struct Context {');b=code.index('unsigned checks=0',a);code=code[:a]+'typedef PathPriorityContext Context;\n#define t_submit t_pathPriority\n'+code[b:]
a=code.index('bool TryPriorityBeforePublish(');b=code.index('void Observe(',a)
policy=(ROOT/'src/pathfind/path_priority_publication.cpp').read_text();records=re.findall(r'\{0x([0-9A-F]+),\{[^}]+\},(\d+)\}',policy)
spec=importlib.util.spec_from_file_location('pe',ROOT/'tools/kenshilib/check_imports.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod);pe=mod.PE(Path('D:/SteamLibrary/steamapps/common/Kenshi/RE_Kenshi/Kenshi_x64.exe'))
fake='struct FakeWindow{uintptr_t rva;unsigned char bytes[24];size_t count;};\nFakeWindow fakeWindows[]={\n'
for r,n in records:
 r=int(r,16);n=int(n);data=pe.data[pe.offset(r):pe.offset(r)+n];fake+=' {0x%X,{%s},%d},\n'%(r,','.join('0x%02x'%x for x in data),n)
fake+='};\n'
fake+='''// Steam-only caller regression fixture; real GOG windows are tested separately.
bool NativeProfileIsGog(){return false;}
namespace GogProfile {const unsigned char* ExpectedBytes(uintptr_t,size_t){return NULL;}}
uintptr_t gameBase=0x140000000ULL;unsigned fakeReads=0,errorLines=0;bool missingRead=false;
void*GameAddr(size_t rva){++fakeReads;if(rva==PATH_PRIORITY_ENQUEUE_RETURN)return (void*)(gameBase+rva);if(missingRead)return NULL;for(size_t i=0;i<sizeof(fakeWindows)/sizeof(fakeWindows[0]);++i)if(fakeWindows[i].rva==rva)return fakeWindows[i].bytes;return NULL;}
#define KLIB_MEMBER(g,p,n,o) ((uintptr_t)(p)+(o))
void LogError(const std::string&){++errorLines;}
'''
start=s.index('static __declspec(thread) PathPriorityContext* t_pathPriority');end=s.index(body('void PathPriorityBeforeEnqueue('))+len(body('void PathPriorityBeforeEnqueue('))
actual=s[start:end]+'\n'+body('void hook_pathReqSubmit(')+'\n'
bridge='''
void Configure(int mask,bool on,bool originals=true){PathPriorityPrepare(on);PathPriorityFinalize((mask&1)!=0,(mask&2)!=0,(mask&4)!=0,originals);}
bool TryPriorityBeforePublish(void*q,void**item,uintptr_t caller){Context*c=t_pathPriority;const bool previous=c&&c->consumed;PathPriorityBeforeEnqueue(q,item,(void*)(gameBase+caller));bool applied=c&&!previous&&c->consumed;if(applied&&harness)++harness->writes;return applied;}
void ProposalSubmit(void*m,void*r,bool high){hook_pathReqSubmit(m,r,high);}
'''
code=code[:a]+fake+actual+bridge+code[b:]
code=code.replace('else hook_pathReqSubmit(manager,request,highPriority);','else LegacySubmit(manager,request,highPriority);')
code=code.replace('Context outer={NULL,&request,q,45,20,false,true,false};','Context outer={&request,q,(void*)(gameBase+NativeSubmitEnqueueReturn),45,20,false,true,false};')
code=code.replace('Context outer={NULL,NULL,NULL,0,0,false,false,false};','Context outer={NULL,NULL,NULL,0,0,false,false,false};')
code=code.replace('harness=&h;capabilityMask=mask;routingEnabled=on;','harness=&h;capabilityMask=mask;routingEnabled=on;Configure(mask,on!=0);')
code=code.replace('capabilityMask=7;routingEnabled=1;harness=NULL;','capabilityMask=7;routingEnabled=1;Configure(7,true);harness=NULL;')
code=code.replace('game::g_hookOrig.orig_pathReqSubmit=NativeSubmit;','game::g_hookOrig.orig_pathReqSubmit=NativeSubmit;Configure(7,true);')
code=code.replace('hook_pathReqSubmit(&mgr,&req,true);C(h.observed==45','LegacySubmit(&mgr,&req,true);C(h.observed==45')
# Actual requestPath body + real water table, including nested original calls.
requestTests=r'''
bool nestedRequest=false,throwRequest=false,sehRequest=false;void*parentRequester=NULL;unsigned requestCalls=0;
bool PlayerRepathTierIsPlayerOwned(uintptr_t){return false;}void FormationFollowNoteRequest(void*){}
REQUEST_BODY
void NativeRequest(void*hc,float*destination,int priority){++requestCalls;C(planner::PlannerWaterCurrentRequester()==hc,"actual water table carries current requester");if(throwRequest)throw std::runtime_error("request unwind");if(sehRequest)RaiseException(0xe0424242,0,0,NULL);if(nestedRequest){nestedRequest=false;void*saved=hc;int tier=squadBoostTier;bool source=squadBoostFromRepathTier;hook_requestPath((void*)0x2222,destination,0);C(planner::PlannerWaterCurrentRequester()==saved&&squadBoostTier==tier&&squadBoostFromRepathTier==source,"actual nested requestPath restores tier source and water requester");}}
unsigned InvokeRequest(){__try{hook_requestPath((void*)0x1111,NULL,2);return 0;}__except(GetExceptionCode()==0xe0424242?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return GetExceptionCode();}}
void RequestScopes(){planner::PlannerWaterTableArm(1,planner::PWE_MATCH);planner::PlannerWaterNoteRequester((void*)0x7777);game::g_hookOrig.orig_requestPath=NativeRequest;squadBoostTier=17;squadBoostFromRepathTier=true;nestedRequest=true;hook_requestPath((void*)0x1111,NULL,2);C(requestCalls==2&&squadBoostTier==17&&squadBoostFromRepathTier&&planner::PlannerWaterCurrentRequester()==(void*)0x7777,"normal request scope restores exact preexisting state");throwRequest=true;bool caught=false;try{hook_requestPath((void*)0x1111,NULL,2);}catch(const std::runtime_error&){caught=true;}throwRequest=false;C(caught&&squadBoostTier==17&&squadBoostFromRepathTier&&planner::PlannerWaterCurrentRequester()==(void*)0x7777,"C++ request unwind restores all exact TLS");sehRequest=true;C(InvokeRequest()==0xe0424242&&squadBoostTier==17&&squadBoostFromRepathTier&&planner::PlannerWaterCurrentRequester()==(void*)0x7777,"SEH request unwind restores all exact TLS");sehRequest=false;planner::PlannerWaterTableArm(0,planner::PWE_MATCH);}
void ProfileAdmission(){unsigned before=fakeReads;PathPriorityPrepare(false);C(fakeReads==before,"OFF does not read native profile");for(size_t i=0;i<sizeof(fakeWindows)/sizeof(fakeWindows[0]);++i){fakeWindows[i].bytes[0]^=1;Configure(7,true);C(!s_priorityReady,"each altered native contract window fails closed");fakeWindows[i].bytes[0]^=1;}missingRead=true;Configure(7,true);C(!s_priorityReady,"unreadable native profile fails closed");missingRead=false;Configure(7,true,false);C(!s_priorityReady,"missing original trampoline fails closed");Configure(7,true);C(s_priorityReady!=0,"full unchanged native profile and pair accepted");}
'''.replace('REQUEST_BODY',body('void hook_requestPath('))
code=code.replace('int main(int argc,char**argv)',requestTests+'\nint main(int argc,char**argv)')
code=code.replace('Predicates();NestedAndUnwind();Threads();','Predicates();NestedAndUnwind();Threads();ProfileAdmission();RequestScopes();')
code=code.replace('\"prototype\"','\"PRODUCTION\"').replace('publication characterization/prototype','publication actual legacy/production')
(OUT/'actual.cpp').write_text(code,encoding='utf-8')
VC=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');SDK=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1');env=dict(os.environ,INCLUDE=f'{VC}/include;{SDK}/Include;{ROOT}/src',LIB=f'{VC}/lib/amd64;{SDK}/Lib/x64')
def run(name,text,pol=policy,success=True):
 folder=OUT/name;folder.mkdir(exist_ok=True);cpp=folder/'actual.cpp';cpp.write_text(text,encoding='utf-8');p=folder/'policy.cpp';p.write_text(pol,encoding='utf-8')
 r=subprocess.run([str(VC/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2','/W3',str(cpp),str(p),str(ROOT/'src/pathfind/player_repath_tier_policy.cpp'),str(ROOT/'src/planner/planner_water_table.cpp'),str(ROOT/'src/planner/plan_policy.cpp'),'/Fo'+str(folder)+'/', '/Fe'+str(folder/'test.exe')],env=env,capture_output=True,text=True);(folder/'build.log').write_text(r.stdout+r.stderr);assert not r.returncode,r.stdout+r.stderr
 r=subprocess.run([str(folder/'test.exe')],capture_output=True,text=True);(folder/'run.log').write_text(r.stdout+r.stderr);print(name,r.stdout[-2000:]);assert (r.returncode==0)==success,(name,r.returncode,r.stdout,r.stderr);return folder/'test.exe'
exe=run('actual',code)
r=subprocess.run([str(exe),'--require-current-priority'],capture_output=True,text=True);(OUT/'legacy-RED.log').write_text(r.stdout+r.stderr);assert r.returncode==1 and 'known RED' in r.stdout;print('Frozen6e15 actual legacy RED confirmed')
if '--skip-mutants' not in sys.argv:
 for name,old,new,target in [
 ('wrong-queue','queue!=c->queue ||','', 'policy'),('wrong-request','if (*item!=c->request) return false;','', 'policy'),
 ('wrong-callsite','ret!=c->enqueueReturn ||','', 'policy'),('repeat-consume','c->consumed ||','', 'policy'),
 ('missing-admission','!c->admitted ||','', 'policy'),('no-submit-leave','PathPriorityLeave(&frame,t_pathPriority);','', 'caller'),
 ('lost-request-tier','squadBoostTier=previousTier;','squadBoostTier=0;', 'caller'),('lost-request-source','squadBoostFromRepathTier=previousSource;','squadBoostFromRepathTier=false;', 'caller'),
 ('lost-water-requester','planner::PlannerWaterNoteRequester(previousRequester);','planner::PlannerWaterNoteRequester(NULL);', 'caller')]:
  text=code if target=='caller' else policy;assert old in text,name;run(name,code.replace(old,new) if target=='caller' else code,policy.replace(old,new) if target=='policy' else policy,False)
# The functionally relevant native enqueue call is first, not hidden in the
# original diagnostic manager/result-queue branch. It still calls original once.
q=(ROOT/'src/pathfind/path_pool_hooks.cpp').read_text();owner=q[q.index('void hook_enqueueThreadSafe('):q.index('__int64 hook_gatesUpdateCodes')]
assert owner.index('PathPriorityBeforeEnqueue(queueBase,itemPtr,_ReturnAddress());')<owner.index('void* mgr = g_sectionMgrPtr;')<owner.index('orig_enqueueThreadSafe(queueBase, itemPtr);')
assert owner.count('orig_enqueueThreadSafe(queueBase, itemPtr);')==1
m=(ROOT/'src/plugin/hook_manifest.cpp').read_text();owner=m[m.index('void InstallHooks('):]
assert owner.index('PathPriorityPrepare(')<owner.index('kInstallSteps[i]')<owner.index('PathPriorityFinalize(')
print('Actual functional enqueue/startup owner ordering PASS')
