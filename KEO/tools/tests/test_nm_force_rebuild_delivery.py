"""Compile exact production mark lifecycle bodies with a fake clock/native grid.

No game headers/process. Invoke after VS2010 vcvarsall amd64.
"""
from pathlib import Path
import re
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'src/navmesh/cache/nm_force_rebuild.cpp'
POLICY = ROOT / 'src/navmesh/cache/nm_force_rebuild_policy.cpp'
OUT = ROOT / 'build/tests/nm_force_rebuild_delivery'


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
    raise ValueError('unclosed function: ' + signature)


def main():
    text = SOURCE.read_text(encoding='utf-8-sig')
    cap = re.search(r'static const int LIVE_CAP = [^;]+;', text).group()
    bodies = '\n'.join(function(text, name) for name in (
        'static int CellIndex(', 'static double AgeSec(',
        'int NmForceRebuildClaimLocked(', 'void NmForceRebuildFinish(',
        'static uintptr_t ZoneContent(', 'static void AddLive(', 'static long MarkCell(',
        'bool NmForceRebuildWantsFront(', 'static void SweepLiveMarks(',
        'void NmForceRebuildOnWorldReset('))
    harness = r'''
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "navmesh/cache/nm_force_rebuild_policy.h"
struct NmQueueLock {};
enum { NM_FORCE_DROPPED=0, NM_FORCE_STORED, NM_FORCE_REFUSED };
enum { NM_SKIP_UNUSED=0 };
enum { OFF_ZONE_COORDS_X=24, OFF_ZONE_COORDS_Y=28, OFF_ZONE_CONTENT=0 };
#define KLIB_MEMBER(G,B,N,O) ((uintptr_t)(B)+(O))
static LONGLONG fakeNow=1000;
static LONGLONG QpcNow() { return fakeNow; }
static double QpcToMs(LONGLONG ticks) { return (double)ticks; }
struct LiveMark { int cell; long word; void* zone; };
static const int MARK_CELLS=NM_REBUILD_GRID*NM_REBUILD_GRID;
static volatile LONG s_markWord[MARK_CELLS];
static volatile LONGLONG s_markQpc[MARK_CELLS];
static volatile LONG s_marked,s_claimed,s_stored,s_refused,s_dropped,s_expired,s_lost;
CAP
static long s_seq=0;
static LiveMark s_live[LIVE_CAP];
static int s_liveCount=0, s_pressCount=0;
static volatile LONG s_holdActive=0,s_dismissOwed=0;
static LONG PressShowApply(NmPressShowEvent,bool) { return 0; }
BODIES
struct Zone { void* content; char pad[16]; int gx,gy; };
static int checks=0,failed=0;
static void Check(bool ok,const char* name) { ++checks; if(!ok) { ++failed; printf("FAIL %s\n",name); } }
static long Mark(Zone* zone) { bool kept=false; return MarkCell(zone->gx*64+zone->gy,zone,fakeNow,false,&kept); }
int main() {
  Zone zones[32]={}; NmQueueLock held;
  for(int i=0;i<32;++i) { zones[i].content=(void*)1; zones[i].gx=i; zones[i].gy=3; }
  long words[32];
  for(int i=0;i<32;++i) words[i]=Mark(&zones[i]);
  Check(s_liveCount<=LIVE_CAP,"live tracking remains bounded");
  for(int i=0;i<32;++i) Check(NmMarkStateOf(s_markWord[i*64+3])==NM_MARK_MARKED,"overflow preserves required cell mark");
  fakeNow+=121000;
  SweepLiveMarks(fakeNow);
  for(int i=0;i<32;++i) {
    long word=0; const int cell=i*64+3;
    Check(NmForceRebuildWantsFront((uintptr_t)&zones[i],0,fakeNow),"old pending mark remains prioritized");
    int claimed=NmForceRebuildClaimLocked(held,(uintptr_t)&zones[i],0,&word);
    Check(claimed==cell && word==NmMarkWord(NmMarkSeqOf(words[i]),NM_MARK_CLAIMED),"old overflowed mark claims forced");
    Check(!NmJobMayReadCache(true,claimed>=0),"same-key cache cannot serve required rebuild");
    long twice=0;
    Check(NmForceRebuildClaimLocked(held,(uintptr_t)&zones[i],0,&twice)==-1,"one claim per invalidation");
    NmForceRebuildFinish(cell,word,NM_FORCE_STORED);
    Check(NmMarkStateOf(s_markWord[cell])==NM_MARK_DONE,"completed invalidation ends");
  }
  Mark(&zones[0]); long first=0;
  int cell=NmForceRebuildClaimLocked(held,(uintptr_t)&zones[0],0,&first);
  const long fresh=Mark(&zones[0]);
  NmForceRebuildFinish(cell,first,NM_FORCE_STORED);
  Check(s_markWord[3]==fresh,"old completion cannot clear newer invalidation");
  zones[0].content=NULL;
  fakeNow+=121000; SweepLiveMarks(fakeNow);
  Check(s_markWord[3]==fresh,"temporary unloaded content does not cancel invalidation");
  zones[0].content=(void*)1;
  long again=0;
  Check(NmForceRebuildClaimLocked(held,(uintptr_t)&zones[0],1,&again)==-1,"partial job does not consume mark");
  Check(NmForceRebuildClaimLocked(held,(uintptr_t)&zones[0],0,&again)==3,"reload still consumes required full rebuild");
  NmForceRebuildOnWorldReset();
  bool empty=true; for(int i=0;i<MARK_CELLS;++i) empty=empty && s_markWord[i]==0;
  Check(empty && s_liveCount==0 && s_pressCount==0,"explicit world reset cancels bounded grid");
  Check(NmHoldDecide(true,1,NM_REBUILD_HOLD_CAP_SEC)==NM_HOLD_CAPPED,"panel hold remains time bounded");
  printf("nm_force_rebuild_delivery: %d checks, %d failed\n",checks,failed);
  return failed?1:0;
}
'''.replace('\nCAP\n', '\n' + cap + '\n').replace('\nBODIES\n', '\n' + bodies + '\n')
    OUT.mkdir(parents=True, exist_ok=True)
    cpp = OUT / 'fixture.cpp'
    cpp.write_text(harness, encoding='utf-8')
    for opt in ('Od', 'O2'):
        exe = OUT / ('fixture_' + opt + '.exe')
        command = ['cl', '/nologo', '/EHsc', '/' + opt, '/Isrc', str(cpp),
                   str(POLICY),
                   '/Fo' + str(OUT) + '\\', '/Fe' + str(exe)]
        compiled = subprocess.run(command, cwd=str(ROOT), text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        print(compiled.stdout, end='')
        if compiled.returncode:
            return compiled.returncode
        run = subprocess.run([str(exe)], cwd=str(ROOT), text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        print(opt + ': ' + run.stdout, end='')
        if run.returncode:
            return run.returncode
    return 0


class ProductionForceDelivery(unittest.TestCase):
    def test_production_lifecycle_od_and_o2(self):
        self.assertEqual(main(), 0)


if __name__ == '__main__':
    unittest.main()
