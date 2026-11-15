"""Private mutation check. Only build-directory copies change; production stays frozen."""
from pathlib import Path
import contextlib
import importlib.util
import io
import re

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/tests/navmesh_delivery_mutants'


def load(name):
    spec=importlib.util.spec_from_file_location(name,ROOT/'tools/tests'/('test_'+name+'.py'))
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module


def change(text,old,new):
    if text.count(old)!=1:raise ValueError('mutation anchor not unique: '+old)
    return text.replace(old,new)


def check(name,module):
    module.OUT=OUT/name
    # The ring runner uses its local out variable; its root is kept, fixture filename is unique.
    output=io.StringIO()
    with contextlib.redirect_stdout(output):rc=module.main()
    body=output.getvalue()
    (OUT/(name+'.log')).write_text(body,encoding='utf-8')
    match=re.search(r'(?:delivery|splice_ring_delivery): (\d+) checks, (\d+) failed',body)
    if rc==0 or not match or int(match.group(2))==0:
        print(body);raise RuntimeError('mutation not refused by executed assertions: '+name)
    print(name+': refused by '+match.group(2)+' executed assertions')


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    force=ROOT/'src/navmesh/cache/nm_force_rebuild.cpp'
    policy=ROOT/'src/navmesh/cache/nm_force_rebuild_policy.cpp'
    base=force.read_text(encoding='utf-8-sig');pol=policy.read_text(encoding='utf-8-sig')
    mutations=[
      ('tracker_cancels_debt',change(base,'if (s_liveCount >= LIVE_CAP)\n\t{','if (s_liveCount >= LIVE_CAP)\n\t{\n InterlockedExchange(&s_markWord[s_live[0].cell],0);'),pol),
      ('claim_expires_debt',base,change(pol,'return NM_CLAIM_CONSUME;','return ageSec > NM_REBUILD_MARK_TTL_SEC ? NM_CLAIM_EXPIRED : NM_CLAIM_CONSUME;')),
      ('sweep_expires_debt',change(base,'const int state = NmMarkStateOf(cur);','const int state = NmMarkStateOf(cur);\n if(state==NM_MARK_MARKED && AgeSec(s_markQpc[m.cell],now)>60) { s_markWord[m.cell]=0; continue; }'),pol),
      ('unload_cancels_debt',change(base,'const int state = NmMarkStateOf(cur);','const int state = NmMarkStateOf(cur);\n if(state==NM_MARK_MARKED && !ZoneContent(m.zone)) { s_markWord[m.cell]=0; continue; }'),pol),
      ('completion_clears_newer',change(base,'InterlockedCompareExchange(&s_markWord[cell], NmMarkWord(NmMarkSeqOf(claimedWord), NM_MARK_DONE), claimedWord);','InterlockedExchange(&s_markWord[cell], NmMarkWord(NmMarkSeqOf(claimedWord), NM_MARK_DONE));'),pol),
      ('reset_keeps_old_world',change(base,'InterlockedExchange(&s_markWord[i], 0);','(void)i;'),pol),
      ('front_forgets_old_debt',base,change(pol,'return jobType == 0 && NmMarkStateOf(word) == NM_MARK_MARKED;','return jobType == 0 && NmMarkStateOf(word) == NM_MARK_MARKED && ageSec<=60;')),
    ]
    for name,source,rule in mutations:
        folder=OUT/name;folder.mkdir(exist_ok=True)
        src=folder/'force.cpp';p=folder/'policy.cpp';src.write_text(source,encoding='utf-8');p.write_text(rule,encoding='utf-8')
        module=load('nm_force_rebuild_delivery');module.SOURCE=src;module.POLICY=p;check(name,module)
    ring=(ROOT/'src/navmesh/construction/splice_ring.cpp').read_text(encoding='utf-8-sig')
    fixture=(ROOT/'tools/tests/splice_ring_delivery.cpp').read_text(encoding='utf-8-sig')
    helper=load('nm_force_rebuild_delivery').function
    old_try=helper(ring,'SplicePush SpliceRingTryPush(')
    non_atomic='''SplicePush SpliceRingTryPush(SpliceRing* r,const float box[6]) {
      LONG w=InterlockedCompareExchange(&r->written,0,0);
      if(w-InterlockedCompareExchange(&r->readPub,0,0)>=SPLICE_RING_SLOTS)return SPLICE_PUSH_FULL;
      LONG idx=InterlockedIncrement(&r->written)-1;
      return ClaimAndFillAt(r,box,idx,true)?SPLICE_PUSH_TAKEN:SPLICE_PUSH_CLAIM_FAILED;
    }\n'''
    rings=[('non_atomic_capacity',change(ring,old_try,non_atomic)),
           ('accepted_abandonment',change(ring,'return Drain(r, out, max, true);','return Drain(r, out, max, false);')),
           ('failed_claim_hole',change(ring,'InterlockedExchange(&s->cancelled, idx);','(void)idx;'))]
    for name,source in rings:
        folder=OUT/name;folder.mkdir(exist_ok=True)
        src=folder/'ring.cpp';src.write_text(source,encoding='utf-8')
        cpp=folder/'fixture.cpp'
        cpp.write_text(change(fixture,'#include "../../src/navmesh/construction/splice_ring.cpp"','#include "'+src.as_posix()+'"'),encoding='utf-8')
        module=load('splice_ring_delivery');module.FIXTURE=cpp;check(name,module)
    print('navmesh delivery mutants: 10/10 refused by executed production assertions')


if __name__=='__main__':main()
