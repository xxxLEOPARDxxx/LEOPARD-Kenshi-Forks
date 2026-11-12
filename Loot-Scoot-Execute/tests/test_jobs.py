"""Compile actual job predicates/scheduler and native attribution hooks with SDK fakes."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile
import gettext

ROOT = Path(__file__).resolve().parents[1]
VC = Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC')
SDK = Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')

def main():
    for locale in ('en_GB','ru_RU','zh_CN'):
        po=ROOT/'Loot-Scoot-Execute/locale'/locale/'LC_MESSAGES/loot_scoot_execute.po'
        with po.with_suffix('.mo').open('rb') as catalog:
            translations=gettext.GNUTranslations(catalog)
        for label in ('Execute own defeats','Execute enemies'):
            assert label in translations._catalog and translations.gettext(label)
        if locale=='ru_RU':
            for code in (*range(0x2010,0x2016),0x2212):
                assert chr(code) not in po.read_text(encoding='utf-8-sig')
                assert chr(code) not in str(translations._catalog)
    print('3 locales: compiled job labels and Russian hyphen checks passed',flush=True)
    source = (ROOT/'LootScootExecuteJobs.inl').read_text(encoding='utf-8')
    scheduler = source[source.index('namespace lse_jobs_runtime'):source.index('// Native damage and medical transitions')]
    attribution = source[source.index('__declspec(thread) static Character* damageActor'):source.index('typedef GameSaveState* (*SaveFn)')]
    persistence = source[source.index('typedef GameSaveState* (*SaveFn)'):source.index('// Discover an exact native class')]
    env = os.environ.copy()
    env['INCLUDE'] = str(VC/'include')+';'+str(SDK/'Include')+';'+str(ROOT)
    env['LIB'] = str(VC/'lib/amd64')+';'+str(SDK/'Lib/x64')
    env['PATH'] = str(VC/'bin/amd64')+';'+env['PATH']
    with tempfile.TemporaryDirectory(prefix='lse-jobs-') as temporary:
        out = Path(temporary)
        (out/'production_jobs_runtime.inc').write_text(scheduler+attribution+persistence+'\n}\n',encoding='utf-8')
        native = source[source.index('static uintptr_t NativeActionAt('):source.index('static uintptr_t NativeAction(const char*')]
        (out/'production_native_action.inc').write_text(native,encoding='utf-8')
        registration = source[source.index('static const unsigned char registration'):source.index('const bool steam')]
        (out/'production_registration_bytes.inc').write_text(registration,encoding='utf-8')
        for name, fixture in [('policy',ROOT/'tests/jobs_policy.cpp'),('runtime',ROOT/'tests/jobs_runtime_fixture.cpp'),('native',ROOT/'tests/jobs_native_fixture.cpp')]:
            command = [str(VC/'bin/amd64/cl.exe'),'/nologo','/EHsc','/W3','/I'+str(out),str(fixture),'/Fe'+str(out/(name+'.exe')),'/Fo'+str(out/(name+'.obj'))]
            result = subprocess.run(command,env=env,cwd=out,text=True,capture_output=True)
            print(result.stdout,end='')
            if result.returncode:
                print(result.stderr);return result.returncode
            arguments=[str(out/(name+'.exe'))]
            if name=='native':arguments.append('D:/SteamLibrary/steamapps/common/Kenshi/RE_Kenshi/Kenshi_x64.exe')
            subprocess.run(arguments,check=True)
    return 0

if __name__=='__main__':
    sys.exit(main())
