"""Exercise exact production crash formatter and stitch detour/observer bodies.

Native stitch and guarded reads are explicit local boundaries, never game calls.
The high-ticket fixture counts actual seq reads instead of relying on timings.
"""
from pathlib import Path
import os
import re
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'build/tests/stitch_diagnostic_boundaries'


def function(text, signature):
    start = text.index(signature)
    brace = text.index('{', start)
    tokens = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', re.S)
    depth = 0
    for token in tokens.finditer(text, brace):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return text[start:token.end()] + '\n'
    raise ValueError('unclosed body: ' + signature)


CRASH = r'''
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "base/fixed_log_buf.h"
typedef FlbExternal SbBuf;
ENTRY
static const int kRingSize=16;
static StitchByteEntry s_ring[kRingSize];
static volatile LONG s_next=0,s_calls=0,s_interior=0,s_skipped=0,s_kept=0,s_nonzero=0;
static int s_state=1; static bool s_actMode=true; static __int64 s_qpcFreq=0;
static const size_t kStitchByteWriteOffset=0x50;
static int seqReads=0,nativeReads=0,checks=0,failed=0;
static LONG Read(volatile LONG* p) {
  for(int i=0;i<16;++i) if(p==&s_ring[i].seq) ++seqReads;
  return InterlockedCompareExchange(p,0,0);
}
static bool SafeReadQword(unsigned __int64,unsigned __int64*) { ++nativeReads;return false; }
BODY
static void Check(bool ok,const char* what) { ++checks;if(!ok){++failed;printf("FAIL %s\n",what);} }
static void Reset(LONG n) { memset(s_ring,0,sizeof(s_ring));s_next=n;seqReads=nativeReads=0; }
static void Publish(LONG n) { StitchByteEntry& e=s_ring[(n-1)%16];e.seq=n;e.number=n;e.uid=7;e.output=0x1000;e.prior=9;e.readable=1;e.skipped=1; }
int main() {
 char out[8192];
 Reset(1000000);StitchByteGuardCrashFormat(out,sizeof(out),4);
 Check(seqReads<=16,"unpublished high session ticket examines at most ring capacity");
 Check(strstr(out,"SBYTE")==NULL && nativeReads==0,"unpublished records are never shown/read");
 Reset(1000000);Publish(1000000);Publish(999999);Publish(999998);Publish(999965);
 StitchByteGuardCrashFormat(out,sizeof(out),4);
 Check(seqReads<=16,"partial publication cannot scan expired session history");
 Check(strstr(out,"SBYTE #999965")==NULL && nativeReads==3,"expired slot generation excluded");
 char* p0=strstr(out,"SBYTE #1000000");char* p1=strstr(out,"SBYTE #999999");char* p2=strstr(out,"SBYTE #999998");
 Check(p0 && p1 && p2 && p0<p1 && p1<p2,"available entries remain newest first");
 Reset(1000000);for(LONG n=999985;n<=1000000;++n) Publish(n);
 StitchByteGuardCrashFormat(out,sizeof(out),2);
 Check(nativeReads==2 && seqReads==2 && strstr(out,"SBYTE #999998")==NULL,"requested row limit preserved");
 seqReads=nativeReads=0;StitchByteGuardCrashFormat(out,sizeof(out),100);
 Check(seqReads==16 && nativeReads==16,"oversized requested rows still bounded by ring");
 Reset(3);Publish(1);Publish(2);Publish(3);StitchByteGuardCrashFormat(out,sizeof(out),4);
 Check(seqReads==3 && nativeReads==3,"startup history stops at ticket one");
 seqReads=nativeReads=0;StitchByteGuardCrashFormat(out,sizeof(out),0);
 Check(seqReads==0 && nativeReads==0,"zero requested entries makes no slot reads");
 Check(StitchByteGuardCrashFormat(NULL,0,4)==0,"empty external storage accepted");
 char tiny[2]={'x','y'};StitchByteGuardCrashFormat(tiny,1,0);
 Check(tiny[0]==0 && tiny[1]=='y',"one byte output terminates without overflow");
 printf("stitch_crash_bound: %d checks, %d failed\n",checks,failed);return failed?1:0;
}
'''

SOURCE = r'''
#include <windows.h>
#include <intrin.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "base/fixed_log_buf.h"
#include "fixes/stitch/stitch_source_policy.h"
#include "fixes/stitch/unstitch_layout.h"
#pragma intrinsic(_ReturnAddress)
static bool detailed=false;static bool DetailedLogEnabled(){return detailed;}
static int checks=0,failed=0,nativeCalls=0,adjCalls=0,readCalls=0,recordCalls=0,generationCalls=0,beats=0,logs=0,stage=0;
static bool nativeFailure=false,readFailure=false,writeNothing=false,flipOff=false;
static const size_t OFF_NAVINST_UID=0x3C;
static unsigned char navA[64],navB[64];
static void* expectedNmg=(void*)1;static void* expectedA=navA;static void* expectedB=navB;
static bool argsOk=true,orderOk=true,adjSplice=false;
static int fakeSite=STITCH_SITE_SPLICE;
static LONG Read(volatile LONG* p){return InterlockedCompareExchange(p,0,0);}
static int SiteOf(void*){return fakeSite;}
static void NmAdjCheckStitch(const void* a,const void* b,bool splice){++adjCalls;argsOk=argsOk&&a==expectedA&&b==expectedB;adjSplice=splice;stage=1;}
static int Orig(void* nmg,void* a,void* b){++nativeCalls;argsOk=argsOk&&nmg==expectedNmg&&a==expectedA&&b==expectedB;orderOk=orderOk&&stage==1;stage=2;if(flipOff)detailed=false;if(nativeFailure)RaiseException(0xE1234567,0,0,NULL);return -7;}
static int (*orig_nmgStitch)(void*,void*,void*)=Orig;
static volatile LONG s_calls=0,s_site[STITCH_SITE_COUNT]={0},s_inFlight=0,s_inFlightMax=0,s_seq=0,s_unreadable=0,s_noWrite=0;
static volatile LONG64 s_inFlightPairs[16];static const int kInFlightSlots=16;static volatile LONG s_inFlightFull=0;
static __int64 Now(){return 12345;}
static bool ReadI32(const void* at,int* out){++readCalls;memcpy(out,at,4);return true;}
static bool ReadPtr(const void* at,const unsigned char** out){++readCalls;memcpy(out,at,sizeof(void*));return true;}
NAVFACTS
static bool ReadNav(const unsigned char*,NavFacts* out){++readCalls;orderOk=orderOk&&stage==2;out->uid=1;out->mesh=writeNothing?NULL:(const unsigned char*)1;out->graph=(const unsigned char*)2;out->gi=(const unsigned char*)3;return !readFailure;}
static bool StitchWrote(const NavFacts* a,const NavFacts* b){return a->mesh && b->mesh && a->uid!=-1 && b->uid!=-1;}
static void NoteGeneration(int,const unsigned char*,__int64){++generationCalls;}
static void RecordSide(const NavFacts*,const NavFacts*,int,int,__int64){++recordCalls;}
static bool s_installed=true,s_addObserved=true;
static LfSlot<StitchUidAdd> addSlots[16];static LfTable<StitchUidAdd> s_add;static volatile LONG s_addSeq=0,s_adds=0;
static __int64 UidKey(int uid){return (__int64)(unsigned int)uid+1;}
static const void* s_lastFiredGi=NULL;static long s_lastFiredCall=-2;static bool s_repeatCall=false;
static double s_nextBeat=0,kBeatSeconds=60;static const char* s_notInstalledWhy="test";
static void EmitHeartbeat(){++beats;}
#define KEO_DETAIL_DEFERRED(x) do{(void)(x);++logs;}while(0)
BODIES
static void Check(bool ok,const char* what){++checks;if(!ok){++failed;printf("FAIL %s\n",what);}}
static void Reset(){nativeCalls=adjCalls=readCalls=recordCalls=generationCalls=stage=0;nativeFailure=readFailure=writeNothing=flipOff=false;argsOk=orderOk=true;memset((void*)s_inFlightPairs,0,sizeof(s_inFlightPairs));s_inFlight=0;}
static bool InvokeFailure(){__try{hook_nmgStitch(expectedNmg,expectedA,expectedB);}__except(EXCEPTION_EXECUTE_HANDLER){return true;}return false;}
int main(){
 Reset();detailed=false;
 Check(hook_nmgStitch(expectedNmg,expectedA,expectedB)==-7,"OFF preserves native return");
 Check(adjCalls==1 && nativeCalls==1 && argsOk && orderOk && adjSplice,"OFF preserves adjacency before exact native arguments");
 Check(readCalls==0 && recordCalls==0 && generationCalls==0 && s_calls==0 && s_seq==0,"OFF performs no forensic read/write/counter path");
 Reset();detailed=true;fakeSite=STITCH_SITE_DISK;
 Check(hook_nmgStitch(expectedNmg,expectedA,expectedB)==-7,"ON preserves native return");
 Check(adjCalls==1 && nativeCalls==1 && argsOk && orderOk && !adjSplice,"ON preserves mandatory order and site classification");
 Check(readCalls==4 && recordCalls==2 && generationCalls==2 && s_inFlight==0,"ON records both sides after native success");
 bool released=true;for(int i=0;i<16;++i)released=released&&s_inFlightPairs[i]==0;
 Check(released,"successful tracked native call releases in-flight pair");
 Reset();detailed=true;readFailure=true;Check(hook_nmgStitch(expectedNmg,expectedA,expectedB)==-7 && recordCalls==0,"unreadable post-state preserves native result");
 Reset();detailed=true;writeNothing=true;Check(hook_nmgStitch(expectedNmg,expectedA,expectedB)==-7 && recordCalls==0,"nonwriting stitch is not fabricated into table");
 Reset();detailed=true;nativeFailure=true;Check(InvokeFailure() && s_inFlight==0 && readCalls==2,"native failure propagates and releases tracked count");
 released=true;for(int i=0;i<16;++i)released=released&&s_inFlightPairs[i]==0;Check(released,"native failure releases pair slot");
 Reset();detailed=false;nativeFailure=true;Check(InvokeFailure() && adjCalls==1 && nativeCalls==1 && readCalls==0,"OFF native failure still propagates after adjacency");
 Reset();detailed=true;flipOff=true;Check(hook_nmgStitch(expectedNmg,expectedA,expectedB)==-7 && s_inFlight==0 && recordCalls==2,"toggle during tracked native call does not abandon its finally");
 unsigned char coll[64]={0},sec[448]={0},slots[48]={0},gi[160]={0},graph[128]={0};
 *(const void**)(coll+OFF_COLL_SLOTS)=slots;*(const void**)(slots+OFF_COLL_SLOT_GRAPHINST)=gi;*(int*)(sec+420)=0;*(int*)(gi+OFF_GI_UID)=42;*(const void**)(gi+OFF_GI_GRAPH)=graph;
 LfInit(&s_add,addSlots,16,8);Reset();detailed=false;
 StitchSourceOnAdd(coll,(__int64)sec,(__int64)gi);
 Check(readCalls==0 && s_add.used==0 && s_adds==0,"OFF insertion observer has no native reads/table writes");
 detailed=true;StitchSourceOnAdd(coll,(__int64)sec,(__int64)gi);
 StitchUidAdd added;bool writing=false;
 Check(s_add.used==1 && LfTableGet(&s_add,43,0,&added,&writing)==LF_GET_HIT && added.graph==(__int64)graph,"ON insertion publishes real lock-free add record");
 int reads=readCalls;StitchSourceOnAdd(NULL,0,0);Check(readCalls==reads,"invalid observer arguments remain refused");
 s_lastFiredGi=(void*)99;s_lastFiredCall=99;detailed=false;StitchSourceBeginFired((void*)3,4);
 Check(s_lastFiredGi==(void*)99 && s_lastFiredCall==99,"OFF drop observer leaves forensic repeat state untouched");
 detailed=true;StitchSourceBeginFired((void*)3,4);StitchSourceBeginFired((void*)3,5);
 Check(s_repeatCall && s_lastFiredCall==5,"ON duplicate teardown classification preserved");
 detailed=false;s_nextBeat=0;StitchSourceTick(100);Check(beats==0 && s_nextBeat==0,"OFF tick avoids heartbeat bookkeeping");
 detailed=true;StitchSourceTick(100);StitchSourceTick(101);Check(beats==1 && s_nextBeat==160,"ON heartbeat throttle preserved");
 detailed=false;s_installed=false;s_nextBeat=0;StitchSourceTick(200);Check(logs==0,"OFF refused-hook tick does no formatting");
 printf("stitch_source_gate: %d checks, %d failed\n",checks,failed);return failed?1:0;
}
'''


def fixtures(crash=None, source=None):
    if crash is None:
        crash = (ROOT / 'src/fixes/stitch/stitch_byte_guard.cpp').read_text(encoding='utf-8-sig')
    if source is None:
        source = (ROOT / 'src/fixes/stitch/stitch_source.cpp').read_text(encoding='utf-8-sig')
    entry = function(crash, 'struct StitchByteEntry').rstrip() + ';\n'
    crash_cpp = CRASH.replace('\nENTRY\n', '\n' + entry).replace('\nBODY\n', '\n' + function(crash, 'size_t StitchByteGuardCrashFormat('))
    bodies = '\n'.join(function(source, sig) for sig in (
        'static int ClaimInFlight(', 'static int CallOrigCounted(', 'static int ReadUidOr(',
        'static int hook_nmgStitch(', 'void StitchSourceOnAdd(',
        'void StitchSourceBeginFired(', 'void StitchSourceTick('))
    source_cpp = SOURCE.replace('\nNAVFACTS\n', '\n' + function(source, 'struct NavFacts').rstrip() + ';\n').replace('\nBODIES\n', '\n' + bodies)
    return crash_cpp, source_cpp


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    vc = Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC')
    sdk = Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
    env = dict(os.environ, INCLUDE=str(vc / 'include') + ';' + str(sdk / 'Include'),
               LIB=str(vc / 'lib/amd64') + ';' + str(sdk / 'Lib/x64'))
    env['PATH'] = str(vc / 'bin/amd64') + ';' + env['PATH']
    failed = 0
    for name, body in zip(('crash_bound', 'source_gate'), fixtures()):
        cpp = OUT / (name + '.cpp')
        cpp.write_text(body, encoding='utf-8')
        for opt in ('Od', 'O2'):
            exe = OUT / (name + '_' + opt + '.exe')
            cmd = [str(vc / 'bin/amd64/cl.exe'), '/nologo', '/EHsc', '/MT', '/' + opt,
                   '/Isrc', str(cpp), '/Fo' + str(OUT) + '\\', '/Fe' + str(exe)]
            if name == 'source_gate':
                cmd.append('src/fixes/stitch/stitch_source_policy.cpp')
            built = subprocess.run(cmd, cwd=ROOT, env=env, capture_output=True)
            (OUT / (name + '_' + opt + '_build.txt')).write_bytes(built.stdout + built.stderr)
            if built.returncode:
                print((built.stdout + built.stderr).decode(errors='replace'))
                return built.returncode
            run = subprocess.run([str(exe)], cwd=ROOT, env=env, capture_output=True)
            print(opt + ': ' + run.stdout.decode(errors='replace'), end='')
            if run.returncode:
                failed += 1
    if not failed and '--mutants' in sys.argv:
        crash = (ROOT / 'src/fixes/stitch/stitch_byte_guard.cpp').read_text(encoding='utf-8-sig')
        source = (ROOT / 'src/fixes/stitch/stitch_source.cpp').read_text(encoding='utf-8-sig')
        cases = [
            ('unbounded_crash_history', 0, ' && examined < kRingSize', ''),
            ('short_crash_ring', 0, 'examined < kRingSize', 'examined < kRingSize - 1'),
            ('crash_wrong_order', 0, 'for (LONG n = Read(&s_next);', 'for (LONG n = Read(&s_next) - 1;'),
            ('off_stitch_forensics', 1, 'if (!DetailedLogEnabled())\n\t\treturn orig_nmgStitch(nmg, a, b);', 'if (false)\n\t\treturn orig_nmgStitch(nmg, a, b);'),
            ('missing_adjacency', 1, '\tNmAdjCheckStitch(a, b, site == STITCH_SITE_SPLICE);', '\t/* missing mandatory adjacency check */'),
            ('native_failure_leaks_count', 1, '\t\tInterlockedDecrement(&s_inFlight);', '\t\t/* missing native cleanup */'),
            ('off_insert_reads', 1, 'void StitchSourceOnAdd(void* collection, __int64 sectionData, __int64 graphInstance)\n{\n\tif (!DetailedLogEnabled()) return;', 'void StitchSourceOnAdd(void* collection, __int64 sectionData, __int64 graphInstance)\n{'),
            ('off_repeat_state', 1, 'void StitchSourceBeginFired(const void* graphInstance, long callNo)\n{\n\tif (!DetailedLogEnabled()) return;', 'void StitchSourceBeginFired(const void* graphInstance, long callNo)\n{'),
            ('off_tick_work', 1, 'void StitchSourceTick(double now)\n{\n\tif (!DetailedLogEnabled()) return;', 'void StitchSourceTick(double now)\n{'),
        ]
        for name, kind, before, after in cases:
            original = (crash, source)[kind]
            if original.count(before) != 1:
                raise AssertionError('mutation must match once: ' + name)
            modified = original.replace(before, after)
            body = fixtures(modified if kind == 0 else crash, modified if kind == 1 else source)[kind]
            cpp = OUT / (name + '.cpp')
            cpp.write_text(body, encoding='utf-8')
            exe = OUT / (name + '.exe')
            cmd = [str(vc / 'bin/amd64/cl.exe'), '/nologo', '/EHsc', '/MT', '/O2', '/Isrc', str(cpp),
                   '/Fo' + str(OUT) + '\\', '/Fe' + str(exe)]
            if kind == 1:
                cmd.append('src/fixes/stitch/stitch_source_policy.cpp')
            built = subprocess.run(cmd, cwd=ROOT, env=env, capture_output=True)
            if built.returncode:
                print((built.stdout + built.stderr).decode(errors='replace'))
                return built.returncode
            run = subprocess.run([str(exe)], cwd=ROOT, env=env, capture_output=True)
            (OUT / (name + '_run.txt')).write_bytes(run.stdout + run.stderr)
            if not run.returncode or b'FAIL ' not in run.stdout:
                raise AssertionError('compiled mutant must fail a behavioral assertion: ' + name)
            print(name + ': expected RED')
        print('PASS 9 compiled production-body mutants')
    return 1 if failed else 0


class ProductionStitchDiagnosticBoundaries(unittest.TestCase):
    def test_real_formatter_detour_and_observers_od_and_o2(self):
        self.assertEqual(main(), 0)


if __name__ == '__main__':
    if '--mutants' in sys.argv:
        raise SystemExit(main())
    unittest.main()
