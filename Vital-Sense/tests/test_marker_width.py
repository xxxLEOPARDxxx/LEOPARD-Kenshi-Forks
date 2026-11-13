"""Exercise production marker placement with explicit measured-text boundaries."""
from pathlib import Path
import os
import subprocess

here = Path(__file__).resolve().parent
root = here.parent
source = (root / 'src/vs_marker_render.cpp').read_text('utf-8-sig')

def function(signature):
    start = source.index(signature)
    end = source.index('{', start)+1
    depth = 1
    while depth:
        depth += (source[end] == '{')-(source[end] == '}')
        end += 1
    return source[start:end]

fixture = r'''
#include <cstdio>
#include <algorithm>
const int kKoMarkerWidthPx=64,kKoMarkerHeightPx=18,kKoBountySymbolMinWidthPx=12,kKoBountySymbolGapPx=1;
const int kKoTextOutlineLayerCount=8,kKoBountyGlowOffsetXPx=-3,kKoBountyGlowOffsetYPx=-3,kKoBountyGlowPaddingPx=3;
namespace MyGUI {
struct IntSize {int width,height;IntSize(int w=0,int h=0):width(w),height(h){}};
struct Widget {
 int left,top,width,height,applies,measures;IntSize measured;
 Widget():left(0),top(0),width(0),height(0),applies(0),measures(0),measured(30,18){}
 void setCoord(int l,int t,int w,int h){left=l;top=t;width=w;height=h;++applies;}
 IntSize getTextSize(){++measures;return measured;}
};
typedef Widget TextBox;typedef Widget ImageBox;
}
struct Visual {
 bool valid;unsigned configRevision;int markerState,markerRelation,totalBounty;
 bool showStateIcon,showStateText,showBountyGlow,showBountySymbol,visible;
 bool positionValid;int positionLeft,positionTop,positionTotalBounty;
 bool positionShowStateIcon,positionShowStateText,positionShowBountySymbol,positionIsBountyOnlyState;
};
typedef Visual KoMarkerVisualState;
struct KoMarkerWidget {
 MyGUI::ImageBox* bountyGlow;MyGUI::ImageBox* icon;MyGUI::TextBox* bountySymbolOutline[8];MyGUI::TextBox* bountySymbol;
 MyGUI::TextBox* fallbackTextOutline[8];MyGUI::TextBox* fallbackText;Visual visualState;
 int textWidthPx,symbolWidthPx;
};
struct RuntimeStateView {struct Config {bool placeBountySymbolBeforeStateIcon;}config;unsigned configRevision;};
int ResolveBountySymbolWidthPx(RuntimeStateView&,int){return 20;}
void GetKoMarkerTextOutlineOffset(size_t n,int* x,int* y){*x=int(n%3)-1;*y=int(n/3)-1;}
// @PRODUCTION@
int checks=0,failures=0;
void Check(bool value,const char* message){++checks;if(!value){++failures;printf("FAIL %s\n",message);}}
int main(){
 RuntimeStateView state={};state.config.placeBountySymbolBeforeStateIcon=true;
 MyGUI::Widget icon,text,symbol,glow,outline[8],symbolOutline[8];KoMarkerWidget marker={};
 marker.icon=&icon;marker.fallbackText=&text;marker.bountySymbol=&symbol;marker.bountyGlow=&glow;
 for(int n=0;n<8;++n){marker.fallbackTextOutline[n]=&outline[n];marker.bountySymbolOutline[n]=&symbolOutline[n];}
 marker.textWidthPx=34;marker.symbolWidthPx=14;
 SetKoMarkerPosition(state,marker,100,50,100,true,true,true,false);
 Check(text.width>=34,"three-character status must not be shortened by bounty symbol");
 Check(text.left>=icon.left+icon.width,"text does not overlap icon");
 Check(icon.left>=symbol.left+symbol.width,"icon does not overlap bounty");
 for(int n=0;n<8;++n)Check(outline[n].width==text.width,"outline and caption get identical content width");
 int writes=text.applies;SetKoMarkerPosition(state,marker,100,50,100,true,true,true,false);
 Check(text.applies==writes,"unchanged marker does not dirty GUI geometry");
 marker.visualState.positionValid=false;marker.textWidthPx=160;
 SetKoMarkerPosition(state,marker,100,50,100,true,true,true,false);
 Check(text.width>=160,"large or Chinese measured glyphs retain their complete width");
 Check(text.measures==0,"placement does not measure text each frame");
 marker.visualState.positionValid=false;
 state.config.placeBountySymbolBeforeStateIcon=false;
 SetKoMarkerPosition(state,marker,100,50,100,true,true,true,false);
 Check(symbol.left>=icon.left+icon.width,"alternate symbol order keeps the icon first");
 Check(text.left>=symbol.left+symbol.width&&text.width>=160,"alternate order keeps full caption after symbol");
 for(int flags=0;flags<8;++flags){
  marker.visualState.positionValid=false;
  bool hasIcon=(flags&1)!=0,hasText=(flags&2)!=0,hasSymbol=(flags&4)!=0;
  int width=ResolveKoMarkerWidth(state,marker,100,hasIcon,hasText,hasSymbol);
  SetKoMarkerPosition(state,marker,100,50,100,hasIcon,hasText,hasSymbol,false);
  if(hasText)Check(text.left+text.width<=100+width,"combined width contains caption for every visible component combination");
  if(hasSymbol)Check(symbol.left+symbol.width<=100+width,"combined width contains bounty for every visible component combination");
 }
 marker.visualState.positionValid=false;marker.symbolWidthPx=100;
 SetKoMarkerPosition(state,marker,100,50,100,false,false,true,true);
 Check(symbol.width>=100,"custom multi-character bounty label is not forced into32px");
 UpdateKoMarkerTextWidths(marker,true,true);
 Check(marker.textWidthPx==34&&marker.symbolWidthPx==34,"actual font glyph width plus outline margin cached");
 int measures=text.measures;UpdateKoMarkerTextWidths(marker,false,false);
 Check(marker.textWidthPx==0&&marker.symbolWidthPx==0&&text.measures==measures,"hidden labels do not request measurement");
 text.measured.width=100000;UpdateKoMarkerTextWidths(marker,true,false);
 Check(marker.textWidthPx==4100,"malformed excessive font widths have bounded geometry");
 UpdateKoMarkerVisualStateCache(marker,state,1,2,100,true,true,false,true);
 marker.visualState.positionValid=true;
 Check(!NeedsKoMarkerVisualRefresh(marker,state,1,2,100,true,true,false,true),"stable visuals retain their measurement cache");
 ++state.configRevision;
 Check(NeedsKoMarkerVisualRefresh(marker,state,1,2,100,true,true,false,true),"config revision requests caption/font measurement refresh");
 UpdateKoMarkerVisualStateCache(marker,state,1,2,100,true,true,false,true);
 Check(!marker.visualState.positionValid,"visual refresh invalidates position cache even without camera motion");
 printf("Vital Sense marker placement: %s (%d checks)\n",failures?"FAIL":"PASS",checks);
 return failures?1:0;
}
'''
signatures = ['bool NeedsKoMarkerVisualRefresh(', 'void UpdateKoMarkerVisualStateCache(', 'void SetKoMarkerPosition(']
for signature in ['int MeasureKoMarkerTextWidth(', 'void UpdateKoMarkerTextWidths(']:
    if signature in source:
        signatures.insert(-1, signature)
for signature in ['int ResolveKoMarkerTextWidth(', 'int ResolveKoMarkerSymbolWidth(', 'int ResolveKoMarkerWidth(']:
    if signature in source:
        signatures.insert(-1, signature)
code = '\n'.join(function(s) for s in signatures)
out = here / '_generated'
out.mkdir(exist_ok=True)
cpp = out / 'marker_width.cpp'
cpp.write_text(fixture.replace('// @PRODUCTION@', code), 'utf-8')
vc = Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC')
sdk = Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env = os.environ.copy()
env['PATH'] = str(vc/'bin/amd64')+';'+env['PATH']
env['INCLUDE'] = str(vc/'include')+';'+str(sdk/'Include')
env['LIB'] = str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64')
exe = out/'marker_width.exe'
subprocess.run([str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/Od',str(cpp),'/Fe'+str(exe),'/Fo'+str(out/'marker_width.obj')],env=env,cwd=out,check=True)
subprocess.run([str(exe)],check=True)
