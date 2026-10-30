"""Read installed XML only; feed its actual plate/insets/colours to the C++ test."""
from pathlib import Path
import xml.etree.ElementTree as ET
game=Path('D:/SteamLibrary/steamapps/common/Kenshi')
out=Path('D:/DEV/Kenshi/build/Organize-the-TraderOfflineTests/header-skin-fixtures.txt')
def colour(text):
    if text.startswith('#'):
        raw=text.lstrip('#');return [int(raw[i:i+2],16)/255 for i in (0,2,4)]+[1.0]
    parts=list(map(float,text.split()));return parts+[1.0]*(4-len(parts))
rows=[]
for base in (game/'data/gui',game/'mods/Dark UI Advanced/gui'):
    layout=ET.parse(base/'layout/Kenshi_InventoryTraderWindow.layout').getroot()
    parents={child:parent for parent in layout.iter() for child in parent}
    data=next(w for w in layout.iter('Widget') if w.get('name')=='datapanel');plate=parents[data]
    pc=list(map(float,plate.get('position_real').split()));dc=list(map(float,data.get('position_real').split()))
    rect=[int(pc[0]*500),int(pc[1]*550),int(pc[2]*500),int(pc[3]*550)]
    child=[int(dc[0]*rect[2]),int(dc[1]*rect[3]),int(dc[2]*rect[2]),int(dc[3]*rect[3])]
    templates=ET.parse(base/'templates/kenshi_templates.xml').getroot()
    standard=next(r for r in templates.iter('Resource') if r.get('name')=='Kenshi_TextboxStandardText')
    props={p.get('key'):p.get('value') for p in standard.iter('Property')}
    skins=ET.parse(base/'skins/kenshi_skins.xml').getroot()
    if plate.get('skin')!='PanelEmpty':
        actual=next(r for r in skins.iter('Resource') if r.get('name')==plate.get('skin'))
        assert any(b.get('type')=='SubSkin' for b in actual.iter('BasisSkin')), 'actual plate background missing'
    foreground=colour(props['TextColour']);assert foreground[3]>0
    palette={p.get('key'):p.get('value') for p in ET.parse(base/'colours/kenshi_colours.xml').getroot().iter('Property')}
    accents=[palette[k] for k in ('Good','Bad')]
    assert all(len(x)==7 and x[0]=='#' and all(c in '0123456789abcdefABCDEF' for c in x[1:]) for x in accents)
    values=[plate.get('skin')]+rect+child+[int('HStretch' in plate.get('align','')),int('HStretch' in data.get('align',''))]+foreground+foreground
    rows.append(' '.join(map(str,values+accents)))
    print('PASS XML: '+str(base)+' plate='+plate.get('skin')+' text='+props['TextColour'])
# Deliberately different input/text roles: copying a white EditBox foreground
# over a white plate must not overwrite the XML's blue standard-text role.
rows.append('NativeWhiteFrame 14 0 203 41 21 11 161 19 0 0 .1 .2 .8 1 1 1 1 1 #123456 #654321')
out.write_text('\n'.join(rows)+'\n',encoding='ascii')
