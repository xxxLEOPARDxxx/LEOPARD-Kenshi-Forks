"""Compile/run the unchanged production loader against packaged MO catalogs.

The Windows filesystem functions and MO parser are real. Only the game locale
service/log sink are test adapters. Each locale runs in a separate process,
preserving the production one-load-per-DLL lifetime contract.
"""
from pathlib import Path
import argparse
import gettext
import hashlib
import json
import os
import re
import subprocess
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(__file__).resolve().parents[1]
DEV = Path('D:/DEV/Kenshi')
GAME = Path('D:/SteamLibrary/steamapps/common/Kenshi')
OUT = DEV / 'build/Organize-the-TraderLanguageFallbackTests'
VC = Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC')
SDK = Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
SHARED = DEV / 'ours/shared'
OUT.mkdir(parents=True, exist_ok=True)
STUB = OUT / 'stubs'
(STUB / 'kenshi').mkdir(parents=True, exist_ok=True)
(STUB / 'kenshi/LocaleInfo.h').write_text('''#pragma once
#include <string>
struct LocaleInfo { std::string id; };
struct LocaleManager {
 static LocaleInfo testLocale; static bool available, localeAvailable; static int reads;
 static LocaleManager* getInstance() { ++reads; static LocaleManager m; return available ? &m : 0; }
 LocaleInfo* getCurrentLocale() { return localeAvailable ? &testLocale : 0; }
};
LocaleInfo LocaleManager::testLocale;
bool LocaleManager::available=true, LocaleManager::localeAvailable=true;
int LocaleManager::reads=0;
''', encoding='ascii')
(STUB / 'Debug.h').write_text('''#pragma once
#include <string>
namespace TestLogs { static int debug=0, errors=0; }
inline void DebugLog(const std::string&) { ++TestLogs::debug; }
inline void ErrorLog(const std::string&) { ++TestLogs::errors; }
''', encoding='ascii')

env = os.environ.copy()
env['INCLUDE'] = f'{VC}/include;{SDK}/Include'
env['LIB'] = f'{VC}/lib/amd64;{SDK}/Lib/x64'
env['PATH'] = f'{VC}/bin/amd64;' + env['PATH']
published_header=subprocess.run(['git','show','b08772c:shared/Localization.h'],
                                cwd=DEV/'ours',check=True,capture_output=True).stdout
published_dir=OUT/'published-loader'
published_dir.mkdir(exist_ok=True)
(published_dir/'Localization.h').write_bytes(published_header)
executables={}
for flavor,include in [('current',SHARED),('published-v7',published_dir)]:
    exe=OUT/f'LanguageFallbackTests-{flavor}.exe'
    flags=['/DTEST_PUBLISHED_LOADER'] if flavor=='published-v7' else []
    subprocess.run([str(VC/'bin/amd64/cl.exe'), '/nologo', '/EHsc', '/MD', '/O2',
                    *flags, '/I'+str(STUB), '/I'+str(include), str(ROOT/'tests/LanguageFallbackTests.cpp'),
                    '/Fe'+str(exe), '/Fo'+str(OUT)+'/', '/link', 'kernel32.lib'],
                   cwd=OUT, env=env, check=True)
    executables[flavor]=exe

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--archive',type=Path)
args=parser.parse_args()
archives=list((DEV/'workshop/nexus').glob('Organize-the-Trader-*.zip'))
archive=args.archive or max(archives,key=lambda p:int(p.stem.rsplit('-',1)[1]))
catalog_evidence={}
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
    mo_entries = [n for n in z.namelist() if n.endswith('.mo')]
    assert len(mo_entries) == 3, mo_entries
    assert not any(n.endswith('.po') for n in z.namelist())
    for n in mo_entries:
        relative = Path(n[n.index('locale/'):])
        target = OUT/relative
        target.parent.mkdir(parents=True, exist_ok=True)
        payload=z.read(n)
        target.write_bytes(payload)
        installed=GAME/'mods/Organize-the-Trader'/relative
        staged=DEV/'workshop/Organize-the-Trader/content'/relative
        assert installed.read_bytes()==staged.read_bytes()==payload, relative
        catalog_evidence[str(relative)]={'bytes':len(payload),'sha256':hashlib.sha256(payload).hexdigest()}
    archived_dll = z.read(next(n for n in z.namelist() if n.endswith('.dll')))

with (OUT/'locale/en_GB/LC_MESSAGES/organize_the_trader.mo').open('rb') as f:
    english = gettext.GNUTranslations(f)._catalog
keys = sorted(k for k in english if k)
assert keys
assert all('\n' not in k and '\t' not in k and '\n' not in english[k]
           and '\t' not in english[k] for k in keys)
(OUT/'english-keys.txt').write_text('\n'.join(k+'\t'+english[k] for k in keys)+'\n', encoding='utf-8')
source = '\n'.join(p.read_text('utf-8-sig') for p in [ROOT/'src/TraderSearchUi.cpp', ROOT/'src/TraderModHub.cpp'])
literal_keys = set(re.findall(r'Tr\("([^"\\]*)"\)', source))
assert literal_keys <= set(keys), literal_keys-set(keys)

locales = ['de_DE', 'de', 'german', 'German', 'de-DE', 'de_DE.UTF-8',
           'fr_FR', 'ja_JP', 'ko_KR', 'es_ES', 'it_IT', 'pl_PL', 'xx_XX',
           'en_GB', 'en_US', 'English', '', '<no-manager>', '<no-locale>']
results=[]
for flavor,exe in executables.items():
    for locale in locales:
        result = subprocess.run([str(exe), locale, str(OUT/'english-keys.txt')],
                                text=True, capture_output=True, check=True)
        line=flavor+': '+result.stdout.strip()
        print(line)
        results.append(line)

# Installed vanilla and Workshop skin layout names remain independent of captions.
layouts=[GAME/'data/gui/layout/Kenshi_InventoryTraderWindow.layout']
workshop = GAME.parents[1]/'workshop/content/233860'
for directory in [workshop,GAME/'mods']:
    for item in directory.iterdir():
        candidate=item/'gui/layout/Kenshi_InventoryTraderWindow.layout'
        if candidate.is_file(): layouts.append(candidate)
required={'ArrangeButton','scrollview_backpack_content','backpack_content','datapanel'}
layout_evidence=[]
for path in layouts:
    names={w.get('name') for w in ET.parse(path).getroot().iter('Widget')}
    assert required <= names, (path, required-names)
    layout_evidence.append(str(path))

def sha(data): return hashlib.sha256(data).hexdigest()
dll_paths=[DEV/'build/Organize-the-Trader/Organize-the-Trader.dll',
           GAME/'mods/Organize-the-Trader/Organize-the-Trader.dll',
           DEV/'workshop/Organize-the-Trader/content/Organize-the-Trader.dll']
report={'production_loader':str(SHARED/'Localization.h'),
        'production_loader_sha256':sha((SHARED/'Localization.h').read_bytes()),
        'published_v7_header_revision':'b08772c:shared/Localization.h',
        'published_v7_header_sha256':sha(published_header),
        'english_catalog_keys':len(keys), 'direct_ui_keys':len(literal_keys),
        'catalogs_identical_in_package_stage_and_install':catalog_evidence,
        'locales':locales, 'results':results, 'layout_paths':layout_evidence,
        'archive':str(archive), 'archive_sha256':sha(archive.read_bytes()),
        'archive_dll_sha256':sha(archived_dll),
        'dlls':{str(p):{'sha256':sha(p.read_bytes()),'bytes':p.stat().st_size} for p in dll_paths},
        'conclusion':'No unsupported-language fallback failure reproduced; German keeps English. This does not reproduce the commenter environment or identify the reported failure.'}
(OUT/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(f'PASS: {len(results)} locale processes, {len(keys)} English keys, '
      f'{len(layout_evidence)} vanilla/Workshop trader layouts; report {OUT / "report.json"}')
