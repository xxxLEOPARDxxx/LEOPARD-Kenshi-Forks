from pathlib import Path
import hashlib,re,gettext,subprocess
base=Path(__file__).resolve().parent/'_generated';base.mkdir(exist_ok=True)
root=Path(r'D:\DEV\Kenshi\forks\LEOPARD-Kenshi-Forks\BetterLooting')
old=subprocess.check_output(['git','-C',str(root.parent),'show','15bdb01:BetterLooting/Source/BetterLooting.cpp']).decode('utf-8-sig')
new=(root/'Source/BetterLooting.cpp').read_text(encoding='utf-8-sig')
def extract(src,name):
 matches=list(re.finditer(r'(?m)^(?:static\s+)?(?:void|bool|int|unsigned|BetterLootingEmergencyStorageResult|std::vector<[^>]+>)\s*\n?'+re.escape(name)+r'\(',src))
 for m in matches:
  opening=src.find('{',m.start());semi=src.find(';',m.start())
  if semi>=0 and semi<opening:continue
  depth=1;i=opening+1
  while depth:depth+=(src[i]=='{')-(src[i]=='}');i+=1
  return src[m.start():i]
 raise ValueError(name)
guard='''
    if (IsBetterLootingBlockedLootInventory(sourceInventory))
    {
        BlTrace("BetterLooting: TRANSFER_SKIP protected player building inventory");
        return;
    }
'''
def nonblank(s):return '\n'.join(line for line in s.splitlines() if line.strip())
assert nonblank(extract(new,'ExecuteLootCandidate').replace(guard,'',1))==nonblank(extract(old,'ExecuteLootCandidate'))
for name in ['EnumerateInventoryCandidates','CollectBetterLootingRadiusTargets','RunBetterLootingStoreAll','EvaluateBetterLootingStoreSourceInventory','QueueSmartLoot']:
 assert extract(new,name)==extract(old,name),name
for name in ['TryBetterLootingNearestCompatiblePlayerStorage','BuildBetterLootingTargetQueue','BuildBetterLootingStoreTargetQueue']:
 current=extract(new,name)
 current=re.sub(r'    static BetterLootingRadiusScratch scratch;.*?    lektor<RootObject\*>& buildingTargets = scratch.buildings;', '    lektor<RootObject*> characterTargets;\n    lektor<RootObject*> buildingTargets;', current,flags=re.S)
 assert nonblank(current)==nonblank(extract(old,name)),name
assert (root/'Source/BetterLootingPanel.cpp').read_text(encoding='utf-8-sig')==subprocess.check_output(['git','-C',str(root.parent),'show','15bdb01:BetterLooting/Source/BetterLootingPanel.cpp']).decode('utf-8-sig').replace('\r\n','\n')
keys=set(re.findall(r'\bTr\("((?:[^"\\]|\\.)*)"\)',new))
keys.update(re.findall(r'\{ "[A-Za-z]+", "([^"]+)", [01] \}',new))
for lang in ['en_GB','ru_RU','zh_CN']:
 p=root/'locale'/lang/'LC_MESSAGES/better_looting.mo'
 with p.open('rb') as f:t=gettext.GNUTranslations(f)
 missing=sorted(k for k in keys if k not in t._catalog or not t._catalog[k])
 assert not missing,(lang,missing)
 print(lang,'all',len(keys),'UI keys translated;',len(t._catalog),'catalog records')
print('PASS: entire original ExecuteLootCandidate preserved after entry-only storage guard; enumeration/sort, Deposit source/runner, radius discovery and direct queue unchanged; preset source byte-identical')
