"""Boundary checks for the real native-weather to display conversion."""
from pathlib import Path
import argparse,os,subprocess
root=Path(__file__).resolve().parents[1]
out=root.parents[2]/'build/CharacterInspectorWeatherCheck';out.mkdir(parents=True,exist_ok=True)
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--baseline',nargs='?',const='7955865',help='Read a Git revision instead of the working header; default is the last pre-fix revision')
args=parser.parse_args()
header=(root/'src/ResistDisplay.h').read_text(encoding='utf-8-sig')
if args.baseline:
 header=subprocess.run(['git','show',args.baseline+':CharacterInspector/src/ResistDisplay.h'],cwd=root,check=True,capture_output=True,text=True).stdout
(out/'ResistDisplayUnderTest.h').write_text(header,encoding='utf-8')
ui=(root/'src/UiText.h').read_text(encoding='utf-8-sig')
def function(signature):
 start=ui.index(signature);brace=ui.index('{',start);depth=1;end=brace+1
 while depth:
  depth+=(ui[end]=='{')-(ui[end]=='}');end+=1
 return ui[start:end]
code=r'''
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>
#include "ResistDisplayUnderTest.h"
namespace MyGUI {struct Widget{};struct Colour{Colour(float,float,float){}};}
std::vector<std::string> lines;
const char* Tr(const char* s){return s;}
void AddColoredLine(MyGUI::Widget*,const char*,float,float,float,float,const char* s,MyGUI::Colour){lines.push_back(s);}
'''
for signature in ('inline MyGUI::Colour MitigationColour(', 'inline float displayPercent(', 'inline float clampDisplayPercent(', 'inline std::string FormatPercentNumber(', 'inline void AddWeatherLine('):
 code+=function(signature)+'\n'
code+=r'''
int main(){const float input[]={-1,0,.15f,.99f,1,1.01f,1.5f,1.55f,1.96f,2,3};
 int failed=0;for(int i=0;i<11;++i){float expected=input[i]<0?0:input[i]*100;
 float actual=resistToPercent(normalizeResist(input[i]));
 if(std::fabs(actual-expected)>.001f){++failed;printf("FAIL native %.3f displays %.2f expected %.2f\n",input[i],actual,expected);}}
 // Keep the established effective-protection display cap at100%. Exercise the
 // real UI row so the data-boundary test cannot mask a second text conversion.
 for(int i=0;i<11;++i){float v=normalizeResist(input[i]),y=0;lines.clear();
  AddWeatherLine(NULL,"Weather",0,y,.03f,.01f,v,v,v,v,v);
  float expected=input[i]<0?0:input[i]*100;if(expected>100)expected=100;
  char text[64];sprintf_s(text,"Acid %.0f%%",expected);
  if(lines.size()!=5 || lines[1]!=text){++failed;printf("FAIL weather UI native %.3f: %s expected %s\n",input[i],lines.size()>1?lines[1].c_str():"missing",text);}}
 printf("22 weather conversion/UI checks, %d failed\n",failed);return failed;}
'''
(out/'weather.cpp').write_text(code,encoding='utf-8')
vc=Path(r'C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path(r'C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=dict(os.environ);env['INCLUDE']=';'.join(map(str,(vc/'include',sdk/'Include',root/'src')))
env['LIB']=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64');env['PATH']=str(vc/'bin/amd64')+';'+env['PATH']
subprocess.run([str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2',str(out/'weather.cpp'),'/Fo'+str(out/'weather.obj'),'/Fe'+str(out/'weather.exe')],env=env,check=True)
subprocess.run([str(out/'weather.exe')],check=True)
