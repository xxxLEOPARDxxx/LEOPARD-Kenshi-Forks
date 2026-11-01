"""Run the actual icon application branch with narrow MyGUI/texture seams.

The native XML identifies the coloured KO sprite; it is not a white tint mask.
No game process, installed config or resource is changed by this test.
"""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile
import xml.etree.ElementTree as ET

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
GAME = Path(r'D:/SteamLibrary/steamapps/common/Kenshi')
VC = Path(r'C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC')
SDK = Path(r'C:/Program Files/Microsoft SDKs/Windows/v7.1')


def function(source, signature):
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


BOUNDARY = r'''
#include <string>
#include <cstdio>
typedef unsigned long DWORD;
namespace MyGUI {
struct Colour {float red,green,blue,alpha;Colour(float r=0,float g=0,float b=0,float a=1):red(r),green(g),blue(b),alpha(a){}};
struct IntCoord {int left,top,width,height;IntCoord(int l=0,int t=0,int w=0,int h=0):left(l),top(t),width(w),height(h){}};
struct IntSize {int width,height;IntSize(int w=0,int h=0):width(w),height(h){}};
struct ImageBox {
 Colour colour;IntCoord coord;std::string texture;unsigned applies;
 ImageBox():applies(0){}
 void setColour(const Colour& c){colour=c;}
 void setImageCoord(const IntCoord& c){coord=c;}
 void setImageTile(const IntSize&){}
};
}
struct CachedKoTarget {enum {STATE_UNCONSCIOUS=1,STATE_RECOVERY_COMA,STATE_DYING,STATE_PLAYING_DEAD,STATE_DEAD,STATE_BOUNTY_ONLY};enum {RELATION_ENEMY,RELATION_ALLY,RELATION_NEUTRAL,RELATION_SQUAD};};
struct Config {
 bool showMarkerIcons;
 std::string customUnconsciousIconTexture,customRecoveryComaIconTexture,customDyingIconTexture,customPlayingDeadIconTexture,customDeadIconTexture;
 DWORD customUnconsciousIconSizePx,customRecoveryComaIconSizePx,customDyingIconSizePx,customPlayingDeadIconSizePx,customDeadIconSizePx;
 MyGUI::Colour enemyMarkerColour,allyMarkerColour,neutralMarkerColour,squadMarkerColour;
 Config():showMarkerIcons(true),customUnconsciousIconSizePx(64),customRecoveryComaIconSizePx(64),customDyingIconSizePx(64),customPlayingDeadIconSizePx(64),customDeadIconSizePx(64),enemyMarkerColour(1,.2f,.2f),allyMarkerColour(.87f,.91f,.35f),neutralMarkerColour(1,.65f,.3f),squadMarkerColour(64/255.f,1,64/255.f){}
};
struct RuntimeStateView {Config config;};
struct KoMarkerWidget {MyGUI::ImageBox* icon;};
// Only native texture loading is replaced. Its separate production-loader
// regression suite covers alias order, preflight, failures and cache bounds.
bool SetKoMarkerIconTexture(RuntimeStateView&,const char*,MyGUI::ImageBox* box,const char* texture){
 ++box->applies;box->texture=std::string(texture)=="missing.png"?"Kenshi_UI.png":texture;
 return box->texture!="Kenshi_UI.png";
}
int checks=0,failures=0;
#define CHECK(x) do{++checks;if(!(x)){++failures;printf("FAIL line %d: %s\n",__LINE__,#x);}}while(0)
bool Equal(const MyGUI::Colour& a,const MyGUI::Colour& b){return a.red==b.red&&a.green==b.green&&a.blue==b.blue&&a.alpha==b.alpha;}
'''

CASES = r'''
int main(){
 RuntimeStateView state;MyGUI::ImageBox icon;KoMarkerWidget widget={&icon};
 const MyGUI::Colour white(1,1,1,1);
 for(int status=1;status<=5;++status)for(int relation=0;relation<4;++relation){
  const bool coloured=status==CachedKoTarget::STATE_UNCONSCIOUS||status==CachedKoTarget::STATE_RECOVERY_COMA;
  const MyGUI::Colour expected=coloured?white:ResolveMarkerColour(state,status,relation);
  std::string* custom=const_cast<std::string*>(ResolveCustomMarkerIconTexture(state,status));
  custom->clear();icon.colour=MyGUI::Colour(0,0,0,0);
  ApplyKoMarkerVisualState(state,"Vital-Sense",widget,status,relation,0,false,false);
  CHECK(icon.texture=="Kenshi_UI.png");CHECK(Equal(icon.colour,expected));
  MyGUI::IntCoord coord=ResolveMarkerIconCoord(status,relation);
  CHECK(icon.coord.left==coord.left&&icon.coord.top==coord.top&&icon.coord.width==coord.width&&icon.coord.height==coord.height);
  *custom="custom.png";
  ApplyKoMarkerVisualState(state,"Vital-Sense",widget,status,relation,0,false,false);
  CHECK(icon.texture=="custom.png"&&Equal(icon.colour,white));
  CHECK(icon.coord.left==0&&icon.coord.top==0&&icon.coord.width==64&&icon.coord.height==64);
  *custom="missing.png";
  ApplyKoMarkerVisualState(state,"Vital-Sense",widget,status,relation,0,false,false);
  CHECK(icon.texture=="Kenshi_UI.png"&&Equal(icon.colour,expected));
  custom->clear();
 }
 unsigned prior=icon.applies;state.config.showMarkerIcons=false;
 ApplyKoMarkerVisualState(state,"Vital-Sense",widget,1,3,0,false,false);CHECK(icon.applies==prior);
 state.config.showMarkerIcons=true;
 ApplyKoMarkerVisualState(state,"Vital-Sense",widget,CachedKoTarget::STATE_BOUNTY_ONLY,3,0,false,false);CHECK(icon.applies==prior);
 widget.icon=NULL;ApplyKoMarkerVisualState(state,"Vital-Sense",widget,1,3,0,false,false);CHECK(icon.applies==prior);
 printf("Vital Sense atlas colour branch: %d checks, %d failures\n",checks,failures);
 return failures?1:0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', action='store_true', help='Use the pinned pre-fix production branch; expected to fail 16 checks.')
    args = parser.parse_args()
    if args.baseline:
        source = subprocess.check_output(['git', '-C', str(ROOT.parent), 'show',
            '27895faaf762fcb548d1ecf0becee7233f40684c:Vital-Sense/src/vs_marker_render.cpp']).decode('utf-8-sig')
    else:
        source = (ROOT / 'src/vs_marker_render.cpp').read_text(encoding='utf-8-sig')
    signatures = [
        'void SetKoMarkerIconColour(', 'void SetKoMarkerIconCoord(',
        'const char* ResolveMarkerIconTexture(', 'MyGUI::IntCoord ResolveMarkerIconCoord(',
        'MyGUI::Colour ResolveMarkerColour(', 'bool IsBountyOnlyMarkerState(',
        'MyGUI::IntCoord ResolveCustomIconCoordFromImageSize(',
        'const std::string* ResolveCustomMarkerIconTexture(',
        'DWORD ResolveCustomMarkerIconSizePx(', 'bool HasCustomMarkerIcon(',
    ]
    if 'MyGUI::Colour ResolveAtlasMarkerIconColour(' in source:
        signatures.append('MyGUI::Colour ResolveAtlasMarkerIconColour(')
    # Exact production icon branch, ending before unrelated bounty/text work.
    apply = function(source, 'void ApplyKoMarkerVisualState(')
    apply = apply[:apply.index('    if (marker.bountyGlow')] + '}\n'
    code = BOUNDARY + '\n'.join(function(source, s) for s in signatures) + apply + CASES
    env = os.environ.copy()
    env['PATH'] = str(VC / 'bin/amd64') + ';' + env['PATH']
    env['INCLUDE'] = str(VC / 'include') + ';' + str(SDK / 'Include')
    env['LIB'] = str(VC / 'lib/amd64') + ';' + str(SDK / 'Lib/x64')
    with tempfile.TemporaryDirectory(prefix='vital-atlas-') as temp:
        out = Path(temp)
        cpp, exe = out / 'atlas.cpp', out / 'atlas.exe'
        cpp.write_text(code, encoding='utf-8')
        subprocess.run([str(VC / 'bin/amd64/cl.exe'), '/nologo', '/EHsc', '/MD', str(cpp), '/Fe'+str(exe)], env=env, cwd=out, check=True)
        result = subprocess.run([str(exe)], check=False)

    # Actual native asset provenance. Dark UI replaces the atlas but inherits
    # the native image definitions when it does not provide an override.
    from PIL import Image
    for resource in (GAME / 'data/gui', GAME / 'mods/Dark UI Advanced/gui'):
        definition = resource / 'images/kenshi_images.xml'
        if not definition.exists():
            definition = GAME / 'data/gui/images/kenshi_images.xml'
        xml = ET.parse(definition).getroot()
        item = xml.find("Resource[@name='pic_PointerKnockout']/Group")
        assert item is not None and item.attrib['texture'] == 'Kenshi_UI.png'
        assert item.attrib['size'] == '32 32' and item.find('Index/Frame').attrib['point'] == '45 122'
        atlas = Image.open(resource / 'gfx/Kenshi_UI.png').convert('RGBA')
        pixels = list(atlas.crop((45, 122, 77, 154)).get_flattened_data())
        opaque = [p for p in pixels if p[3] >= 240]
        assert len(opaque) == 497
        # A neutral vertex colour preserves all authored RGB/alpha. Green
        # multiplication reproduces the dark icon without altering transparency.
        tinted = [(r*64/255, g, b*64/255, a) for r,g,b,a in opaque]
        dark = sum(max(p[:3]) < 64 for p in tinted)
        assert dark > len(opaque)*.75
        assert [p[3] for p in tinted] == [p[3] for p in opaque]
        assert sum(p[0] for p in opaque) > 3*sum(p[1] for p in opaque)
        print(f'{resource}: native KO XML/crop/alpha confirmed; old green tint darkens {dark}/{len(opaque)} opaque pixels')
    raise SystemExit(result.returncode)


if __name__ == '__main__':
    main()
