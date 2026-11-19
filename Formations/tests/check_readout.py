"""Check formation popup geometry and the stock/Dark UI attachment and skin contracts."""
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[1]
game = Path('D:/SteamLibrary/steamapps/common/Kenshi')
out = root.parents[2] / 'build/FormationsReadoutCheck'
out.mkdir(parents=True, exist_ok=True)

for theme in (game / 'data', game / 'mods/Dark UI Advanced'):
    gui = theme / 'gui'
    layout = ET.parse(gui / 'layout/Kenshi_MainPanel.layout')
    assert layout.find(".//Widget[@name='ExtendingPanel']/Widget[@name='NamePanel']") is not None
    assert layout.find(".//Widget[@name='ExtendingPanel']/Widget[@name='BountyPanel']") is not None
    skin = ET.parse(gui / 'skins/kenshi_skins.xml')
    assert skin.find(".//Resource[@name='Kenshi_FloatingPanelThinSkin']") is not None
    templates = ET.parse(gui / 'templates/kenshi_templates.xml')
    assert templates.find(".//Resource[@name='Kenshi_GenericTextBoxFlat']") is not None
    style = templates.find(".//Resource[@name='Kenshi_TextboxStandardText']")
    assert style is not None
    assert style.find(".//Property[@key='FontName']") is not None
    assert style.find(".//Property[@key='TextColour']") is not None

source = out / 'readout.cpp'
source.write_text(r'''
#include <cassert>
#include "ReadoutLayout.h"
int main() {
    readout::Rect rect{};
    assert(readout::Place(0, 550, -1, 179, 271, rect));
    assert(rect.x == 0 && rect.y + rect.height == 550);
    assert(rect.width == 179 + readout::Padding * 2);
    assert(rect.height == 271 + readout::Padding * 2);
    assert(readout::Place(-3, 550, 488, 113, 256, rect));
    assert(rect.x == 0 && rect.y + rect.height == 488);
    assert(readout::Place(10, 450, 560, 55, 200, rect));
    assert(rect.x == 10 && rect.y + rect.height == 450);
    assert(rect.width >= readout::MinWidth);
    assert(!readout::Place(0, 234, -1, 120, 236, rect));
    assert(readout::Place(0, 234, -1, 120, 205, rect));
    assert(!readout::Place(0, 550, -1, 0, 240, rect));
    assert(!readout::Place(0, 550, -1, 120, 1000000, rect));
}
''', encoding='utf-8')
vc = Path('C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat')
bat = out / 'build.cmd'
bat.write_text(f'@echo off\ncall "{vc}" >nul\ncl /nologo /EHsc /std:c++17 /MT /I"{root}" readout.cpp /Fe:readout.exe\n', encoding='utf-8')
subprocess.run(['cmd.exe', '/c', str(bat)], cwd=out, check=True)
subprocess.run([str(out / 'readout.exe')], cwd=out, check=True)
print('Formations readout geometry, vanilla and Dark UI contracts PASS')
