"""Reaggregate retained files only. Never starts or attaches to Kenshi/WPR."""
from pathlib import Path
import argparse,csv,json,hashlib,collections
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--stage',type=Path,default=Path('D:/SteamLibrary/steamapps/common/Kenshi/_codex_work/keo-resource-cache-20261114'))
parser.add_argument('--output',type=Path,default=Path('build/pose_cpu/retained-review.json'))
args=parser.parse_args();result={'newGameDataCollected':False,'inputs':{},'slowRecordSets':{}}
def read(path):
 data=path.read_bytes();result['inputs'][str(path)]={'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()};return data.decode('utf-8-sig')
fields=('meshCallbackUs','texture2DUs','textureArrayUs','morphCreateUs','morphSwitchUs','poseBlendUs','animationBodyUs','attachmentBuildUs')
for name in ('runtime-appearance-preparation-first','runtime-narrow-texture-startup-first','runtime-texture-off-lagoon-mourn-first'):
 data=json.loads(read(args.stage/name/'analysis.json'));records=[r for r in data['slow']if isinstance(r,dict)];elapsed=sum(r['elapsedUs']for r in records)
 result['slowRecordSets'][name]={'retainedCalls':len(records),'selectedElapsedUs':elapsed,'nestedInclusiveChildUs':{k:sum(r.get(k,0)for r in records)for k in fields},'maxSelectedCallUs':max(r['elapsedUs']for r in records),'limitation':'Selected slow wall-time records, nested children overlap. Predates later fixes; not present-day CPU time or whole-save attribution.'}
folder=args.stage/'etw_tools/captures/game-20261115-040423-dd6772a7/decoded';metadata=dict(csv.reader(read(folder/'summary.csv').splitlines()));frames=collections.defaultdict(list)
for r in csv.DictReader(read(folder/'stack-frames.csv').splitlines()):
 module=r['module'].replace('\\','/').rsplit('/',1)[-1].lower();frames[int(r['stack'])].append((module,int(r['rva'],16)if r['rva'] else 0))
tags=collections.Counter();samples=list(csv.DictReader(read(folder/'cpu-samples.csv').splitlines()))
for r in samples:
 seen=set()
 for module,address in frames.get(int(r['stack']),[]):
  if module=='kenshi_x64.exe':
   for name,a,b in [('saveParserRange',0x6c1330,0x6cb000),('morphCreate',0x71a80,0x71d74),('resourceFinishRange',0x44bba0,0x44cc00)]:
    if a<=address<b:seen.add(name)
  elif module=='ogremain_x64.dll'and 0x1a9be0<=address<0x1a9ea1:seen.add('poseMath')
 for name in seen:tags[name]+=1
result['etw']={'sessionDurationMs':float(metadata['sessionDurationMs']),'firstRetainedTargetSampleMs':float(metadata['firstTargetSampleMs']),'lastRetainedTargetSampleMs':float(metadata['lastTargetSampleMs']),'retainedTargetSampleSpanMs':float(metadata['lastTargetSampleMs'])-float(metadata['firstTargetSampleMs']),'cpuSamples':len(samples),'missingSampleStacks':int(metadata['missingSampleStacks']),'namedRangeSampleTags':{k:tags[k]for k in ('saveParserRange','morphCreate','poseMath','resourceFinishRange')},'limitation':'No whole-save CPU ranking. Retained suffix is dominated by rendering/steady work; zero range tags do not prove the work never occurred elsewhere.'}
args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(result,indent=2),encoding='utf-8');print(json.dumps({k:v for k,v in result.items()if k!='inputs'},indent=2))
