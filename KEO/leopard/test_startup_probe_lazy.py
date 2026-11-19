"""Compile production init/report with fake install backend; never run the game."""
from pathlib import Path
import os, subprocess, sys
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/startup_probe_lazy_test'
OUT.mkdir(parents=True,exist_ok=True)
source=(ROOT/'src/resources/resource_load_probe.cpp').read_text()
# Expose the production init section to this fixture. Earlier TEST exclusions
# still omit native includes and real AddHook helpers. Rebind dependencies only
# around that init section, leaving the actual initializer/report logic intact.
marker='#ifndef RESOURCE_LOAD_PROBE_TEST\nvoid ResourceLoadProbeInit(){'
if marker not in source:
    marker='#if !defined(RESOURCE_LOAD_PROBE_TEST) || defined(RESOURCE_LOAD_PROBE_INIT_TEST)\n'
    assert source.count(marker)>=1
    start=source.index(marker)
else:
    start=source.index(marker)
end=source.index('\n#endif',start)+len('\n#endif')
section=source[start:end]
section=section.replace('#ifndef RESOURCE_LOAD_PROBE_TEST','#if 1',1)
binding='\n#define GetModuleHandleA TestGetModuleHandleA\n#define GetProcAddress TestGetProcAddress\n#define ImageMatches TestImageMatches\n#define AdmitModule TestAdmitModule\n'
unbinding='\n#undef GetModuleHandleA\n#undef GetProcAddress\n#undef ImageMatches\n#undef AdmitModule\n'
source=source[:start]+binding+section+unbinding+source[end:]
(OUT/'startup_probe_production.cpp').write_text(source)
VC=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC')
SDK=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=dict(os.environ,INCLUDE=str(VC/'include')+';'+str(SDK/'Include'),LIB=str(VC/'lib/amd64')+';'+str(SDK/'Lib/x64'))
env['PATH']=str(VC/'bin/amd64')+';'+str(SDK/'Bin')+';'+env['PATH']
for opt in ('Od','O2'):
    exe=OUT/('startup-'+opt+'.exe')
    command=[str(VC/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MT','/'+opt,'/I'+str(ROOT/'src'),'/I'+str(OUT),
             str(ROOT/'leopard/test_startup_probe_lazy.cpp'),'/Fo'+str(OUT/('startup-'+opt+'.obj')),'/Fe'+str(exe)]
    result=subprocess.run(command,cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    (OUT/('build-'+opt+'.log')).write_bytes(result.stdout)
    if result.returncode:print(result.stdout.decode(errors='replace'));raise SystemExit(result.returncode)
    for mode in ('gog','gog-tail-fail','gog-create-hook-fail','off','on','early','worker','capture-fail','capture-hook-fail','switch-hook-fail','switch-contract-fail','timing-fail','fallback','reentrant'):
        result=subprocess.run([str(exe),mode],cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        (OUT/(opt+'-'+mode+'.log')).write_bytes(result.stdout)
        print(result.stdout.decode(errors='replace').strip())
        if result.returncode:raise SystemExit(result.returncode)
if '--mutants' in sys.argv:
    capture=next(line for line in source.splitlines() if 'texture2DInstalled=InstallAdmitted("texture2D"' in line)
    cases=[
        ('capture-debug-gated','if(!IsMainThread()||captureAttempted)return;',
         'if(!DetailedLogEnabled()||!IsMainThread()||captureAttempted)return;','off','mandatory texture completion hook attempted exactly once'),
        ('timing-debug-ignored','if(!DetailedLogEnabled()||!IsMainThread()||!captureInitializationComplete||diagnosticsAttempted)return;',
         'if(!IsMainThread()||!captureInitializationComplete||diagnosticsAttempted)return;','off','Debug OFF installs only mandatory capture'),
        ('capture-omitted',capture,'    texture2DInstalled=false;','off','mandatory texture completion hook attempted exactly once'),
        ('switch-capture-omitted',next(line for line in source.splitlines() if 'morphSwitchInstalled=InstallAdmitted("morphSwitch"' in line),
         '    morphSwitchInstalled=false;','off','mandatory final mesh hook second and exactly once'),
        ('timing-reinstalled','    diagnosticsAttempted=true;','    diagnosticsAttempted=false;','off','failures and repeated OFF ON never retry installed or refused seams'),
        ('heartbeat-lazy-omitted','    EnsureDiagnosticHooks(); // existing owner heartbeat, before the report throttle',
         '    /* missing lazy owner admission */','off','first Debug ON heartbeat installs timing before report throttle'),
        ('initialization-gate-ignored','||!captureInitializationComplete||diagnosticsAttempted',
         '||diagnosticsAttempted','early','report before mandatory init cannot install timing hooks'),
        ('capture-published-too-early',capture,'    captureInitializationComplete=true;\n'+capture,
         'reentrant','mandatory final mesh hook second and exactly once'),
        ('gog-enabled-diagnostics','    if(NativeProfileIsGog())return;',
         '    if(false)return;','gog','GOG heartbeat/toggle never retries or enables optional hooks'),
        ('gog-tail-proof-ignored','if(gog&&texture2DSite&&!AdmitModule(exe,stamp,size,0x44aa20,gogTextureContract))',
         'if(false)','gog-tail-fail','GOG refusal is local to its texture contract'),
    ]
    try:
        for name,before,after,mode,reason in cases:
            assert source.count(before)==1,(name,'mutation location')
            (OUT/'startup_probe_production.cpp').write_text(source.replace(before,after))
            exe=OUT/(name+'.exe')
            command=[str(VC/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MT','/O2','/I'+str(ROOT/'src'),'/I'+str(OUT),
                     str(ROOT/'leopard/test_startup_probe_lazy.cpp'),'/Fo'+str(OUT/(name+'.obj')),'/Fe'+str(exe)]
            result=subprocess.run(command,cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
            (OUT/(name+'-build.log')).write_bytes(result.stdout)
            assert result.returncode==0,(name,'build failure is not accepted RED')
            result=subprocess.run([str(exe),mode],cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
            (OUT/(name+'-run.log')).write_bytes(result.stdout)
            assert result.returncode!=0 and ('FAIL '+reason).encode() in result.stdout,(name,'expected assertion not observed')
            print(name+': expected RED')
    finally:
        (OUT/'startup_probe_production.cpp').write_text(source)
    print('PASS',len(cases),'compiled production-init mutation controls')
