"""VC10 actual HUD policy tests and real native SDK compile. No renderer claims."""
from pathlib import Path
import os, subprocess, xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/resource-warm-hud';OUT.mkdir(parents=True,exist_ok=True)
VC=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC')
SDK=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=dict(os.environ,INCLUDE=f'{VC}/include;{SDK}/Include;{ROOT}/src;D:/DEV/Kenshi/deps/KenshiLib_keo/Include;D:/DEV/Kenshi/deps/KenshiLib_keo/Include/ogre;D:/DEV/Kenshi/deps/boost_1_60_0',LIB=f'{VC}/lib/amd64;{SDK}/Lib/x64')
def run(name,args,passes=True):
 r=subprocess.run(args,env=env,cwd=ROOT,capture_output=True,text=True)
 (OUT/(name+'.log')).write_text(r.stdout+r.stderr,encoding='utf-8')
 print(r.stdout+r.stderr)
 if (r.returncode==0)!=passes:raise SystemExit(r.returncode or 1)
run('policy-build',[str(VC/'bin/amd64/cl.exe'),'/nologo','/EHsc','/O2',str(ROOT/'leopard/test_resource_warm_hud.cpp'),f'/Fo{OUT}/policy.obj',f'/Fe{OUT}/policy.exe'])
run('policy-run',[str(OUT/'policy.exe')])
if (ROOT/'src/resources/resource_warm_hud.cpp').exists():
 source=(ROOT/'src/resources/resource_warm_hud.cpp').read_text(encoding='utf-8')
 (OUT/'resource_warm_hud_actual_runtime.inc').write_text('\n'.join(line for line in source.splitlines() if not line.startswith('#include ')),encoding='utf-8')
 run('runtime-build',[str(VC/'bin/amd64/cl.exe'),'/nologo','/EHsc','/O2',f'/I{OUT}',str(ROOT/'leopard/test_resource_warm_hud_runtime.cpp'),f'/Fo{OUT}/runtime.obj',f'/Fe{OUT}/runtime.exe'])
 run('runtime-run',[str(OUT/'runtime.exe')])
 for name,old,new in [
  ('no-throttle','if(!resource_warm_hud::Due(now,state.at,state.sampled))return;',''),
  ('no-caption-cache','if(captions[i]!=lines.text[i])','if(true)'),
  ('no-row-unlink','for(int i=0;i<3;++i)if(widget==rows[i])rows[i]=NULL;',''),
  ('leak-widgets','if(gui==liveGui&&manager==liveManager)for(int i=0;i<3;++i)if(rows[i])liveGui->destroyWidget(rows[i]);',''),
  ('retain-observer','if(manager&&manager==liveManager)manager->unregisterUnlinker(this);',''),
  ('template-as-skin','createWidget<MyGUI::TextBox>("Kenshi_TextBoxEmptySkin"','createWidget<MyGUI::TextBox>("Kenshi_TextboxStandardText"'),
  ('missing-font-assignment','rows[i]->setFontName("Kenshi_StandardFont_Medium");',''),
  ('missing-readiness','if(!resources||!skins||!fonts||!skins->isExist("Kenshi_TextBoxEmptySkin")||\n            !resources->isExist("Kenshi_StandardFont_Medium")||!fonts->getByName("Kenshi_StandardFont_Medium")){','if(false){')]:
  assert old in source,name
  mutated=source.replace(old,new)
  (OUT/'resource_warm_hud_actual_runtime.inc').write_text('\n'.join(line for line in mutated.splitlines() if not line.startswith('#include ')),encoding='utf-8')
  run(name+'-build',[str(VC/'bin/amd64/cl.exe'),'/nologo','/EHsc','/O2',f'/I{OUT}',str(ROOT/'leopard/test_resource_warm_hud_runtime.cpp'),f'/Fo{OUT}/{name}.obj',f'/Fe{OUT}/{name}.exe'])
  run(name+'-run',[str(OUT/(name+'.exe'))],False)
 (OUT/'resource_warm_hud_actual_runtime.inc').write_text('\n'.join(line for line in source.splitlines() if not line.startswith('#include ')),encoding='utf-8')
 print('Eight compiled HUD font/lifecycle/performance negative controls rejected')
 run('native-compile',[str(VC/'bin/amd64/cl.exe'),'/nologo','/c','/EHsc','/O2','/GL','/MD','/W3','/DNDEBUG','/DWIN32_LEAN_AND_MEAN','/DBOOST_ALL_NO_LIB',str(ROOT/'src/resources/resource_warm_hud.cpp'),f'/Fo{OUT}/native.obj'])
 # Verify actual vanilla and installed Dark UI resource definitions. The native
 # raster/palette contrast still requires in-game visual acceptance.
 game=ROOT.parents[1]
 for label,path in [('vanilla',game/'data/gui/skins/kenshi_skins.xml'),('Dark UI Advanced',game/'mods/Dark UI Advanced/gui/skins/kenshi_skins.xml')]:
  xml=ET.parse(path).getroot()
  resources={node.attrib.get('name'):node for node in xml.iter('Resource')}
  text=resources['Kenshi_TextBoxEmptySkin']
  assert text.attrib['type']=='ResourceSkin',label
  assert 'Kenshi_TextboxStandardText' not in resources,label
  assert any(n.attrib.get('type')=='SimpleText' for n in text.findall('BasisSkin')),label
  assert any(n.attrib.get('type')=='SubSkin' for n in text.findall('BasisSkin')),label
  print(label,'actual ResourceSkin with native text part inspected PASS')
 templates=ET.parse(game/'data/gui/templates/kenshi_templates.xml').getroot()
 template=next(n for n in templates.iter('Resource') if n.attrib.get('name')=='Kenshi_TextboxStandardText')
 assert template.attrib['type']=='ResourceLayout'
 widget=template.find('Widget');assert widget.attrib['skin']=='Kenshi_TextBoxEmptySkin'
 assert any(n.attrib.get('key')=='FontName' and n.attrib.get('value')=='Kenshi_StandardFont_Medium' for n in widget.findall('Property'))
 fonts=ET.parse(game/'data/gui/fonts/kenshi_fonts.xml').getroot()
 assert any(n.attrib.get('name')=='Kenshi_StandardFont_Medium' and n.attrib.get('type')=='ResourceTrueTypeFont' for n in fonts.iter('Resource'))
 assert 'createWidget<MyGUI::TextBox>("Kenshi_TextBoxEmptySkin"' in source
 assert 'createWidget<MyGUI::TextBox>("Kenshi_TextboxStandardText"' not in source
 assert 'createWidget<MyGUI::Widget>' not in source
 print('Actual template -> skin + explicit font contract and three text-only roots inspected PASS')
