"""Compile actual job predicates/scheduler and native attribution hooks with SDK fakes."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile
import gettext
import hashlib
import struct

ROOT = Path(__file__).resolve().parents[1]
VC = Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC')
SDK = Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')

def check_native_abi():
    sys.path.insert(0,'D:/DEV/Kenshi/tools/_native_python')
    import pefile
    profiles = [
        ('Steam',Path('D:/SteamLibrary/steamapps/common/Kenshi/RE_Kenshi/Kenshi_x64.exe'),
         '504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e',0x61C8C0,0x60DF70,0x51B7B0,0x60F2F0),
        ('GOG',Path('D:/SteamLibrary/steamapps/common/Kenshi/_codex_work/20261105-keo-gog-native/re_kenshi/kenshi_GOG_x64.exe'),
         '2defc287c813798b9682b93acf68d340da895cf5164d342145ba0faeb9024681',0x61CBD0,0x60E280,0x51BAC0,0x60F600)]
    for name,path,digest,ctor,register,node,score in profiles:
        assert hashlib.sha256(path.read_bytes()).hexdigest()==digest, name+' native executable identity'
        pe=pefile.PE(str(path),fast_load=True)
        windows=[
            (ctor+0xD0,'48898708010000'), # score pointer at108
            (ctor+0xE0,'48898710010000'), # second PMF qword at110
            (ctor+0xF0,'48898718010000'), # third PMF qword at118
            (ctor+0x13B,'4088af34010000'), # native hasAction at134
            (register+0x42,'0f10070f2945e0f20f104f10f20f114df0'), # copies16+8 PMF bytes
            (node+0xD0,'4038bd34010000750a4883bd2801000000'), # action/subtask planner admission
            (score+0x1F,'48638810010000'), # native this adjustment
            (score+0x30,'ff9008010000')] # native score pointer call
        for address,hex_bytes in windows:
            expected=bytes.fromhex(hex_bytes)
            assert pe.get_data(address,len(expected))==expected, f'{name} ABI bytes {address:x}'
        # Both exact executables implement scoreNone as a literal1.0 return.
        none=score+0x317D0
        body=pe.get_data(none,9)
        assert body[:4]==bytes.fromhex('f30f1005') and body[8]==0xC3, name+' scoreNone body'
        constant=none+8+struct.unpack('<i',body[4:8])[0]
        assert struct.unpack('<f',pe.get_data(constant,4))[0]==1.0, name+' native scoreNone returns1'
        print(name+': exact native PMF24, TaskData134 and planner/action ABI verified offline',flush=True)

def main():
    check_native_abi()
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
    production = (ROOT/'Loot-Scoot-Execute.cpp').read_text(encoding='utf-8')
    sdk_includes = '\n'.join(line for line in production.splitlines() if line.startswith('#include <kenshi/'))
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
        (out/'production_jobs_sdk.inc').write_text(sdk_includes+'\n',encoding='utf-8')
        (out/'production_jobs_abi.inc').write_text(source[:source.index('namespace lse_jobs_runtime')],encoding='utf-8')
        collection=production[production.index('static size_t CollectExecuteAllTargets('):production.index('static void ClearExecuteAllHoverTint()')]
        (out/'production_execute_all_collection.inc').write_text(collection,encoding='utf-8')
        native = source[source.index('static uintptr_t NativeActionAt('):source.index('static uintptr_t NativeAction(const char*')]
        (out/'production_native_action.inc').write_text(native,encoding='utf-8')
        registration = source[source.index('static const unsigned char registration'):source.index('const bool steam')]
        (out/'production_registration_bytes.inc').write_text(registration,encoding='utf-8')
        for name, fixture in [('policy',ROOT/'tests/jobs_policy.cpp'),('runtime',ROOT/'tests/jobs_runtime_fixture.cpp'),('native',ROOT/'tests/jobs_native_fixture.cpp'),('collection',ROOT/'tests/jobs_collection_fixture.cpp')]:
            command = [str(VC/'bin/amd64/cl.exe'),'/nologo','/EHsc','/W3','/I'+str(out),str(fixture),'/Fe'+str(out/(name+'.exe')),'/Fo'+str(out/(name+'.obj'))]
            result = subprocess.run(command,env=env,cwd=out,text=True,capture_output=True)
            print(result.stdout,end='')
            if result.returncode:
                print(result.stderr);return result.returncode
            arguments=[str(out/(name+'.exe'))]
            if name=='native':arguments.append('D:/SteamLibrary/steamapps/common/Kenshi/RE_Kenshi/Kenshi_x64.exe')
            subprocess.run(arguments,check=True)
            if name=='collection':
                local=collection.replace('static lektor<RootObject*> nearbyTargets;',
                                         'lektor<RootObject*> nearbyTargets;')
                assert local!=collection
                (out/'production_execute_all_collection.inc').write_text(local,encoding='utf-8')
                result=subprocess.run(command,env=env,cwd=out,text=True,capture_output=True)
                assert not result.returncode,result.stdout+result.stderr
                result=subprocess.run(arguments,text=True,capture_output=True)
                assert result.returncode and 'destructions==0' in result.stdout,result.stdout+result.stderr
                (out/'production_execute_all_collection.inc').write_text(collection,encoding='utf-8')
                print('Per-call lektor buffer mutant rejected before accessing destroyed scratch',flush=True)
        # Compile the real production include order and layout guards against
        # the same SDK used by build.ps1, rather than another fake TaskData.
        sdk_includes_args=['/I'+str(out),'/ID:/DEV/Kenshi/KenshiLib-src/Include',
                           '/ID:/DEV/Kenshi/KenshiLib-src/Include/ogre','/ID:/DEV/Kenshi/deps/boost_1_60_0']
        command=[str(VC/'bin/amd64/cl.exe'),'/nologo','/EHsc','/O2','/MD','/DUNICODE','/D_UNICODE',
                 '/DBOOST_ALL_NO_LIB','/DBOOST_ERROR_CODE_HEADER_ONLY',*sdk_includes_args,
                 str(ROOT/'tests/jobs_score_abi_fixture.cpp'),'/Fe'+str(out/'abi.exe'),'/Fo'+str(out/'abi.obj')]
        result=subprocess.run(command,env=env,cwd=out,text=True,capture_output=True)
        print(result.stdout,end='')
        if result.returncode: print(result.stderr);return result.returncode
        subprocess.run([str(out/'abi.exe')],check=True)
        # Restoring the former include order must fail the real ABI guards.
        prior=sdk_includes.replace('#include <kenshi/Tasker.h>\n','',1)
        (out/'production_jobs_sdk.inc').write_text(prior+'\n',encoding='utf-8')
        result=subprocess.run(command,env=env,cwd=out,text=True,capture_output=True)
        assert result.returncode and 'Native AI score PMF ABI' in result.stdout, result.stdout+result.stderr
        print('Former production include order rejected by native ABI guards',flush=True)
    return 0

if __name__=='__main__':
    sys.exit(main())
