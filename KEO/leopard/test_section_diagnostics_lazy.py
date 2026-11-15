"""Actual production section-hook initializer and wrapper controls. No game access."""
from pathlib import Path
import os, subprocess, sys, json, hashlib
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/section_diagnostics_lazy_test';OUT.mkdir(parents=True,exist_ok=True)
paths=['src/plugin/hook_manifest.cpp','src/plugin/hook_manifest_policy.cpp','src/plugin/hook_manifest_policy.h','src/fixes/streaming/navmesh_life.cpp','src/pathfind/path_guard_hooks.cpp']
raw={p:(ROOT/p).read_text() for p in paths}
def function(src,name):
    begin=src.index(name);brace=src.index('{',begin);depth=1;end=brace+1
    while depth:
        depth+=(src[end]=='{')-(src[end]=='}');end+=1
    return src[begin:end]
def bodies(sources):
    manifest=sources[paths[0]];policy=sources[paths[1]];life=sources[paths[3]];add=sources[paths[4]]
    # Initial baseline still has both diagnostic installs in their old groups.
    if 'static void InstallSectionDiagnosticHooks(' in manifest:
        start=manifest.index('static bool s_sectionDiagnosticsInitializationReached')
        end=manifest.index('// Path-worker-pool pass-through',start)
        init=manifest[start:end]
    else:
        init=function(manifest,'static void InstallPathExtractHooks(')
        init+='\nstatic void InstallSectionDiagnosticHooks(int* installed,int* total){InstallNavMeshLife(installed,total);InstallPathExtractHooks(installed,total);}\nvoid InstallSectionDiagnosticsOnDemand(){}\n'
    return '\n'.join([function(policy,'bool HookWantEval('),function(life,'void InstallNavMeshLife('),init,function(add,'void hook_addInstance('),function(life,'static __int64 hook_removeInstance(')])
fixture=r'''
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <stdexcept>
#include "plugin/hook_manifest_policy.h"
#include "base/log_admission.h"
static bool detail=false,refuseAdd=false,refuseRemove=false,reenter=false,throws=false;
static int attempts[3]={0},errors=0,notes=0,checks=0,nativeCalls=0,addNotes=0,stitchNotes=0,samples=0;
static int* activeInstalled=NULL;
enum {HOOK_ADD_INSTANCE,HOOK_REMOVE_INSTANCE,HOOK_CONTENT_STREAM_CALLEE_0X8869};
static const size_t RVA_HKAI_NAVMESH_INSTANCE_VFTABLE=1,RVA_HKAI_NAVMESH_VFTABLE=2;
static int vtables[3];
static const void* s_instVtbl=NULL;static const void* s_meshVtbl=NULL;
static LONGLONG s_qpf=0,s_nextBeat=0;
static volatile LONG s_rets=0,addInstanceHookCalls=0;
static void* observed[5];
static void Check(bool ok,const char* why){++checks;if(!ok){std::printf("FAIL %s\n",why);std::exit(1);}}
bool DetailedLogEnabled(){return detail;}
void LogMsg(const std::string&){}
void LogError(const std::string&){++errors;}
static void EmitHeartbeat(){}
static void MaybeHeartbeat(){}
static int Sample(const unsigned char*,const void*,const char*,int){++samples;return 0;}
static void* GameAddr(size_t rva){return &vtables[rva];}
static bool HookRowWanted(int id){
 HookWantInputs in={};in.navMeshLife=true;in.pathfindDiag=true;in.sectionStamp=true;
#ifdef SECTION_HAS_DETAIL_WANT
 in.detailedLog=detail;
#endif
 if(id==HOOK_ADD_INSTANCE)return HookWantEval(HOOK_WANT_ADD_INSTANCE,in);
 if(id==HOOK_REMOVE_INSTANCE)return HookWantEval(HOOK_WANT_NAVMESH_LIFE,in);
 return false;
}
static void OriginalAdd(void*c,__int64 i,__int64 p,__int64 g,int f){++nativeCalls;observed[0]=c;observed[1]=(void*)i;observed[2]=(void*)p;observed[3]=(void*)g;observed[4]=(void*)(size_t)f;if(throws)throw std::runtime_error("native");}
static __int64 OriginalRemove(__int64 c,__int64 i,__int64 g){++nativeCalls;observed[0]=(void*)c;observed[1]=(void*)i;observed[2]=(void*)g;if(throws)throw std::runtime_error("native");return 0x123456789abcdefLL;}
namespace game { struct Originals {void(*orig_addInstance)(void*,__int64,__int64,__int64,int);void(*orig_contentStreamCallee0x8869)();} g_hookOrig;}
static __int64(*orig_removeInstance)(__int64,__int64,__int64)=NULL;
void hook_addInstance(void*,__int64,__int64,__int64,int);
static __int64 hook_removeInstance(__int64,__int64,__int64);
void hook_contentStreamCallee0x8869(){}
void InstallSectionDiagnosticsOnDemand();
void InstallNavMeshLife(int*,int*);
static void StitchSourceNoteAddObserver(){++notes;}
static void NavMeshLifeOnAdd(void*,__int64){++addNotes;}
static void StitchSourceOnAdd(void*,__int64,__int64){++stitchNotes;}
template<class F> const char* HookInstall(int id,F,F* original,int* installed,bool verify){
 ++attempts[id];
#ifdef SECTION_HAS_DETAIL_WANT
 Check(verify,"fresh native prologue check before every section hook attempt");
#endif
 if(id==HOOK_REMOVE_INSTANCE)Check(s_instVtbl==&vtables[1]&&s_meshVtbl==&vtables[2]&&s_qpf>0,"native sampler data initialized before enabling hook");
 if(reenter)InstallSectionDiagnosticsOnDemand();
 if((id==HOOK_ADD_INSTANCE&&refuseAdd)||(id==HOOK_REMOVE_INSTANCE&&refuseRemove))return "prologue";
 *original=reinterpret_cast<F>(id==HOOK_ADD_INSTANCE?(void*)&OriginalAdd:(void*)&OriginalRemove);
 if(installed)++*installed;return NULL;
}
// PRODUCTION
int main(int argc,char**argv){
 const std::string mode=argc>1?argv[1]:"off";
 if(mode=="early"){detail=true;InstallSectionDiagnosticsOnDemand();Check(attempts[0]+attempts[1]==0,"heartbeat before startup phase cannot install");return 0;}
 detail=mode!="off";refuseAdd=mode=="add-refusal";refuseRemove=mode=="remove-refusal";reenter=mode=="reentrant";
#ifdef SECTION_HAS_DETAIL_WANT
 for(int bits=0;bits<16;++bits){HookWantInputs in={};in.detailedLog=(bits&1)!=0;in.pathfindDiag=(bits&2)!=0;in.sectionStamp=(bits&4)!=0;in.navMeshLife=(bits&8)!=0;
 Check(HookWantEval(HOOK_WANT_ADD_INSTANCE,in)==(in.detailedLog&&((in.pathfindDiag&&in.sectionStamp)||in.navMeshLife)),"all add diagnostic want combinations");
 Check(HookWantEval(HOOK_WANT_NAVMESH_LIFE,in)==(in.detailedLog&&in.navMeshLife),"all retire diagnostic want combinations");}
 Check(HookRowWanted(HOOK_ADD_INSTANCE)==detail,"add startup total matches diagnostic request");
 Check(HookRowWanted(HOOK_REMOVE_INSTANCE)==detail,"remove startup total matches diagnostic request");
#endif
 int installed=0;activeInstalled=&installed;
 InstallSectionDiagnosticHooks(&installed,NULL);
 if(mode=="off"){
  Check(attempts[0]+attempts[1]==0,"OFF startup installs no pure section hooks");
  Check(installed==0,"OFF diagnostics do not inflate startup total");
  InstallSectionDiagnosticsOnDemand();Check(attempts[0]+attempts[1]==0,"OFF heartbeat remains dormant");
  detail=true;InstallSectionDiagnosticsOnDemand();
  Check(installed==0,"lazy installation never retains startup count pointer");
 }else Check(installed==(refuseAdd||refuseRemove?1:2),"ON startup preserves independent installed counts");
 Check(attempts[0]==1&&attempts[1]==1,"first enable admits both diagnostic hooks once");
 reenter=false;
 for(int i=0;i<4;++i){detail=!detail;InstallSectionDiagnosticsOnDemand();InstallSectionDiagnosticHooks(NULL,NULL);}
 Check(attempts[0]==1&&attempts[1]==1,"toggle and reentry never retry successful or refused hooks");
 Check(notes==(refuseAdd?0:1),"add observer published only after successful native enable");
 Check(errors==((refuseAdd?1:0)+(refuseRemove?1:0)),"independent admission errors remain visible");
 detail=false;int calls=nativeCalls;
 if(!refuseAdd){
  hook_addInstance((void*)1,2,3,4,5);Check(nativeCalls==calls+1,"OFF add native called exactly once");
  Check(observed[0]==(void*)1&&observed[1]==(void*)2&&observed[2]==(void*)3&&observed[3]==(void*)4&&observed[4]==(void*)5,"OFF add preserves every native argument");
  Check(addInstanceHookCalls+addNotes+stitchNotes==0,"OFF add wrapper omits pure diagnostic work");
  throws=true;bool caught=false;try{hook_addInstance(NULL,0,0,0,0);}catch(const std::runtime_error&){caught=true;}Check(caught,"add native exception propagates");throws=false;
  detail=true;hook_addInstance(NULL,0,0,0,0);Check(addInstanceHookCalls==1&&addNotes==1&&stitchNotes==1,"ON add consumers preserved");detail=false;
 }
 if(!refuseRemove){
  calls=nativeCalls;Check(hook_removeInstance(11,12,13)==0x123456789abcdefLL&&nativeCalls==calls+1,"OFF retire native result and exactly once call");
  Check(observed[0]==(void*)11&&observed[1]==(void*)12&&observed[2]==(void*)13,"OFF retire preserves every native argument");
  Check(s_rets==0&&samples==0,"OFF retire omits sampler");
  throws=true;bool caught=false;try{hook_removeInstance(1,2,3);}catch(const std::runtime_error&){caught=true;}Check(caught,"retire native exception propagates");throws=false;
 }
 std::printf("PASS %s %d actual production section controls\n",mode.c_str(),checks);return 0;
}
'''
VC=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');SDK=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=dict(os.environ,INCLUDE=str(VC/'include')+';'+str(SDK/'Include'),LIB=str(VC/'lib/amd64')+';'+str(SDK/'Lib/x64'));env['PATH']=str(VC/'bin/amd64')+';'+env['PATH']
def execute(sources,name,opt,mode):
 cpp=OUT/(name+'.cpp');cpp.write_text(fixture.replace('// PRODUCTION',bodies(sources)));exe=OUT/(name+'.exe')
 cmd=[str(VC/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MT','/'+opt,'/I'+str(ROOT/'src')]
 if 'bool detailedLog;' in sources[paths[2]]:cmd+=['/DSECTION_HAS_DETAIL_WANT']
 cmd+=[str(cpp),'/Fo'+str(OUT/(name+'.obj')),'/Fe'+str(exe)]
 result=subprocess.run(cmd,cwd=OUT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(OUT/(name+'-build.log')).write_bytes(result.stdout)
 if result.returncode:print(result.stdout.decode(errors='replace'));raise SystemExit(result.returncode)
 result=subprocess.run([str(exe),mode],cwd=OUT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(OUT/(name+'-'+mode+'.log')).write_bytes(result.stdout);return result
for opt in ('Od','O2'):
 for mode in ('off','on','early','add-refusal','remove-refusal','reentrant'):
  result=execute(raw,opt+'-'+mode,opt,mode);print(result.stdout.decode(errors='replace').strip())
  if result.returncode:raise SystemExit(result.returncode)
if '--mutants' in sys.argv:
 cases=[
  ('eager',paths[0],'if (!DetailedLogEnabled() || s_sectionDiagnosticsAttempted) return;','if (s_sectionDiagnosticsAttempted) return;','off','first enable admits both diagnostic hooks once'),
  ('early',paths[0],'if (s_sectionDiagnosticsInitializationReached && DetailedLogEnabled() && !s_sectionDiagnosticsAttempted)','if (DetailedLogEnabled() && !s_sectionDiagnosticsAttempted)','early','heartbeat before startup phase cannot install'),
  ('retry',paths[0],'s_sectionDiagnosticsAttempted = true;','s_sectionDiagnosticsAttempted = false;','add-refusal','toggle and reentry never retry successful or refused hooks'),
  ('lazy-missing',paths[0],'InstallSectionDiagnosticHooks(NULL, NULL);','; /* lazy admission missing */','off','first enable admits both diagnostic hooks once'),
  ('add-gate-missing',paths[1],'return in.detailedLog && ((in.pathfindDiag && in.sectionStamp) || in.navMeshLife);','return (in.pathfindDiag && in.sectionStamp) || in.navMeshLife;','off','all add diagnostic want combinations'),
  ('remove-gate-missing',paths[1],'return in.detailedLog && in.navMeshLife;','return in.navMeshLife;','off','all retire diagnostic want combinations'),
  ('add-work-off',paths[4],'if (!DetailedLogEnabled()) return;\n\tInterlockedIncrement(&addInstanceHookCalls);','/* no OFF bypass */\n\tInterlockedIncrement(&addInstanceHookCalls);','on','OFF add wrapper omits pure diagnostic work'),
 ]
 for name,path,before,after,mode,expected in cases:
  assert raw[path].count(before)==1,(name,'mutation location')
  changed=dict(raw);changed[path]=raw[path].replace(before,after)
  result=execute(changed,'mutant-'+name,'O2',mode)
  assert result.returncode and (expected is None or ('FAIL '+expected).encode() in result.stdout),(name,result.stdout)
  print(name+': expected RED')
 print('PASS compiled production lazy-admission negative controls')
(OUT/'source-sha256.json').write_text(json.dumps({p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in paths},indent=2)+'\n')
