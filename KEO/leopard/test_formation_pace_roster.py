from pathlib import Path
import os,re,subprocess,sys
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/formation-pace-roster';OUT.mkdir(parents=True,exist_ok=True)
def section(text,marker,trailing=False):
 start=text.index(marker);brace=text.index('{',start);at=brace+1;depth=1
 while depth:depth+=(text[at]=='{')-(text[at]=='}');at+=1
 return text[start:at+(1 if trailing else 0)]
formation=(ROOT/'src/movement/formation.cpp').read_text();pace=(ROOT/'src/movement/formation_pace.cpp').read_text();header=(ROOT/'src/movement/formation.h').read_text()
groups='const int MAX_FORMATION_GROUPS=8,MAX_FORMATION_MEMBERS_LIMIT=64;\n'+section(header,'struct FormationMember {',True)+'\n'+section(header,'struct FormationGroup {',True)
assert re.search(r'const int MAX_FORMATION_GROUPS\s*=\s*8;',header)
poll=section(formation,'void PollFormationGroups()')
pub=section(pace,'void FormationPaceFrameBegin()')+'\n'+section(pace,'void FormationPaceFramePublish()')
template=(ROOT/'leopard/test_formation_pace_roster.cpp').read_text()
def compose(p):return template.replace('// PRODUCTION_GROUPS',groups).replace('// PRODUCTION_PACE',pub).replace('// PRODUCTION_POLL',p)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=dict(os.environ,INCLUDE=str(vc/'include')+';'+str(sdk/'Include'),LIB=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64'));env['PATH']=str(vc/'bin/amd64')+';'+env['PATH']
def run(name,text,opt):
 cpp=OUT/(name+'.cpp');cpp.write_text(text);exe=OUT/(name+'.exe')
 args=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MT','/'+opt,'/I'+str(ROOT/'src'),str(cpp),str(ROOT/'src/movement/formation_pace_policy.cpp'),'/Fo'+str(OUT)+'\\','/Fe'+str(exe)]
 build=subprocess.run(args,cwd=ROOT,env=env,capture_output=True);(OUT/(name+'-build.txt')).write_bytes(build.stdout+build.stderr);assert build.returncode==0,(build.stdout+build.stderr).decode(errors='replace')
 p=subprocess.run([str(exe)],cwd=ROOT,env=env,capture_output=True);(OUT/(name+'-run.txt')).write_bytes(p.stdout+p.stderr);return p
for opt in ('Od','O2'):
 p=run('red-original' if '--expect-red' in sys.argv else opt,compose(poll),opt);print(p.stdout.decode(errors='replace')+p.stderr.decode(errors='replace'),end='')
 if '--expect-red' in sys.argv:
  assert p.returncode!=0 and b'PaceTableRead(&s_table,paced)==1.0f' in p.stderr;print('expected production roster RED');raise SystemExit(0)
 assert p.returncode==0
if '--mutants' in sys.argv:
 cases=[
  ('staged-factors-not-cleared','\tFormationPaceFrameBegin();','\t/* missing staging clear */'),
  ('missing-owner-not-published','if (!playerIntf)\n\t{\n\t\tFormationPaceFramePublish();','if (!playerIntf)\n\t{\n\t\t/* missing publication */'),
  ('invalid-roster-not-published','if (!scStuff || scCount == 0 || scCount > 200)\n\t{\n\t\tFormationPaceFramePublish();','if (!scStuff || scCount == 0 || scCount > 200)\n\t{\n\t\t/* missing publication */'),
  ('group-state-cleared','if (!playerIntf)\n\t{','if (!playerIntf)\n\t{\n\t\tformationGroups[0].count = 0;'),
  ('overcap-roster-polled','scCount > 200','false'),
  ('empty-roster-polled','scCount == 0','false'),
  ('valid-output-always-empty','FormationFollowPoll(now, scCount, scStuff);\n\tFormationPaceFramePublish();','FormationFollowPoll(now, scCount, scStuff);\n\tFormationPaceFrameBegin();\n\tFormationPaceFramePublish();'),
 ]
 for name,before,after in cases:
  assert poll.count(before)==1,(name,'unique production mutation')
  p=run(name,compose(poll.replace(before,after)),'O2')
  assert p.returncode!=0 and b'FAIL ' in p.stderr,(name,'expected compiled assertion failure')
  print(name+': expected RED')
 print('PASS',len(cases),'compiled actual production roster mutants')
