"""Actual-production GPU cache teardown fixture; no game, renderer or collectors."""
from pathlib import Path
import os,re,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/gpu-cache-drop-index';OUT.mkdir(parents=True,exist_ok=True)
header=(ROOT/'src/render/gpu_param_cache.h').read_text()
impl=re.sub(r'^#include "render/gpu_param_cache.h"\n','',(ROOT/'src/render/gpu_param_cache.cpp').read_text(),flags=re.M)
fixture=(ROOT/'leopard/test_gpu_cache_drop_index.cpp').read_text()
assert header.count('Entry m_slots[SLOTS];')==1
instrumented=header.replace('Entry m_slots[SLOTS];','CountedArray<Entry, SLOTS> m_slots;')
instrumented=instrumented.replace('unsigned short m_mapNext[SLOTS];','CountedLinks<unsigned short, SLOTS> m_mapNext;')
if 'm_mapHeads' in header:instrumented='#define INDEX_PRESENT\n'+instrumented
if 'm_mapPrev' in header:instrumented='#define PREV_PRESENT\n'+instrumented
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=os.environ.copy();env['INCLUDE']=str(vc/'include')+';'+str(sdk/'Include');env['LIB']=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64');env['PATH']=';'.join([str(vc/'bin/amd64'),str(vc/'../Common7/IDE'),str(sdk/'Bin'),env['PATH']])
def run(name,mode,body):
    cpp=OUT/(name+'.cpp');cpp.write_text(fixture.replace('// CACHE_HEADER',instrumented).replace('// CACHE_IMPLEMENTATION',body));exe=OUT/(name+'.exe')
    build=subprocess.run([str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MT','/'+mode,str(cpp),'/Fo'+str(OUT)+'\\','/Fe'+str(exe)],env=env,cwd=ROOT,capture_output=True)
    (OUT/(name+'-build.txt')).write_bytes(build.stdout+build.stderr);assert build.returncode==0,build.stdout.decode(errors='replace')+build.stderr.decode(errors='replace')
    try:result=subprocess.run([str(exe)],env=env,cwd=ROOT,capture_output=True,timeout=15)
    except subprocess.TimeoutExpired as error:
        (OUT/(name+'-run.txt')).write_text('FAIL: fixture timeout (chain nontermination)\n');print(name+': runtime RED: fixture timeout');return 124
    (OUT/(name+'-run.txt')).write_bytes(result.stdout+result.stderr);print(name+': '+result.stdout.decode(errors='replace'),end='');return result.returncode
for mode in ['Od','O2']:
    rc=run(mode,mode,impl)
    if '--expect-red' in sys.argv:assert rc!=0;print('expected runtime RED');break
    assert rc==0
if '--mutants' in sys.argv:
    mutants=[
        ('all-slot-scan',re.sub(r'int GpuParamCache::DropMap\(const void\* map\)\s*\{.*?\n\}', 'int GpuParamCache::DropMap(const void* map) { int dropped=0; for(int i=0;i<SLOTS;++i) { if(m_slots[i].map!=map)continue;memset(&m_slots[i],0,sizeof(m_slots[i]));++dropped;}return dropped;\n}',impl,flags=re.S)),
        ('counter-never-populates',impl.replace('++m_count;','/* missed increment */;')),
        ('counter-never-retires',impl.replace('--m_count;','/* missed decrement */;')),
        ('collision-erases-neighbor',impl.replace('if (e.map != map)','if (false)')),
        ('overwrite-leaves-old-chain',impl.replace('UnlinkMapSlot(index);','/* missed unlink */;')),
        ('clear-keeps-chain',impl.replace('memset(m_mapHeads, 0, sizeof(m_mapHeads));','/* stale map heads */')),
        ('unlink-misses-successor-backlink',impl.replace('m_mapPrev[next - 1] = previous;','/* missed reverse link */;')),
        ('drop-misses-successor-backlink',impl.replace('m_mapPrev[m_mapNext[i] - 1] = m_mapPrev[i];','/* missed reverse link */;')),
        ('rewrite-links-twice',impl.replace('if (slot->map != map)','if (true)').replace('if (slot->map)\n\t\t\tUnlinkMapSlot(index);','if (slot->map && slot->map != map)\n\t\t\tUnlinkMapSlot(index);')),
    ]
    for name,body in mutants:
        assert body!=impl,name
        assert run(name,'O2',body)!=0,name+' survived'
        print(name+': compiled, expected runtime RED')
