"""Actual installed-file guards and optional compiled production mutants.

No game/process control. `--mutants` checks the real inclusion fixture rejects
independently broken cache, budget, lifetime and forwarding implementations.
"""
from pathlib import Path
import hashlib, importlib.util, os, re, struct, subprocess, sys

ROOT=Path(__file__).resolve().parents[1]
GAME=Path(os.environ.get('KENSHI_ROOT','D:/SteamLibrary/steamapps/common/Kenshi'))
spec=importlib.util.spec_from_file_location('pe',ROOT/'tools/kenshilib/check_imports.py')
pem=importlib.util.module_from_spec(spec);spec.loader.exec_module(pem)
ogre=pem.PE(GAME/'OgreMain_x64.dll');steam=pem.PE(GAME/'RE_Kenshi/Kenshi_x64.exe');renderer=pem.PE(GAME/'RenderSystem_Direct3D11_x64.dll')
source=(ROOT/'src/resources/resource_texture_retention.cpp').read_text()
assert 'GetProcAddress(ogre,"?load@Resource@Ogre@@UEAAX_N@Z")' not in source, 'render-pass Resource::load seam must not be hooked'

def read(pe,rva,n):
    at=pe.offset(rva);return pe.data[at:at+n]

def exports(pe):
    at=pe.offset(pe.directories[0][0]);count=pe.u32(at+24)
    names=pe.offset(pe.u32(at+32));ords=pe.offset(pe.u32(at+36));funcs=pe.offset(pe.u32(at+28))
    return {pe.string(pe.u32(names+4*i)):pe.u32(funcs+4*pe.u16(ords+2*i)) for i in range(count)}

syms=exports(ogre);checks=0
def check(ok):
    global checks
    assert ok
    checks+=1

check(hashlib.sha256(ogre.data).hexdigest()=='7d3888d21398647519f10a89b1f300d8606f761ad159e6021a797b1b9b3df5f6')
check(hashlib.sha256(steam.data).hexdigest()=='504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e')
pe=struct.unpack_from('<I',ogre.data,0x3c)[0]
check(struct.unpack_from('<I',ogre.data,pe+8)[0]==0x5ca5f929)
check(struct.unpack_from('<I',ogre.data,pe+24+56)[0]==0x9c9000)
check('stamp!=0x5ca5f929' in source and 'size!=0x9c9000' in source)
symbol='?load@TextureManager@Ogre@@UEAA?AV?$SharedPtr@VTexture@Ogre@@@2@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@0W4TextureType@2@HM_NW4PixelFormat@2@2@Z'
check(syms[symbol]==0x379890)
check('GetProcAddress(ogre,"'+symbol+'")' in source)
expected=bytes(int(v.strip(),16) for v in re.search(r'managerBytes\[32\]=\{([^}]+)',source)[1].split(','))
check(len(expected)==32 and read(ogre,syms[symbol],32)==expected)
check('reinterpret_cast<uintptr_t>(managerTarget)-ogreBase!=0x379890' in source)
check(hashlib.sha256(renderer.data).hexdigest()=='5f224882ded5b6d3f68f14d6bb4e1762267bbb602b7b9733bd153663d1541bb9')
rp=struct.unpack_from('<I',renderer.data,0x3c)[0]
check(struct.unpack_from('<I',renderer.data,rp+8)[0]==0x5ca5f9e9)
check(struct.unpack_from('<I',renderer.data,rp+24+56)[0]==0xbf000)
check('rendererStamp!=0x5ca5f9e9' in source and 'rendererSize!=0xbf000' in source)
impl=bytes(int(v.strip(),16) for v in re.search(r'implBytes\[32\]=\{([^}]+)',source)[1].split(','))
check(len(impl)==32 and read(renderer,0x47d20,32)==impl)
rbase=renderer.image_base
check(struct.unpack('<Q',read(renderer,0x7ce08+0x58,8))[0]-rbase==0x47d20)
check(struct.unpack('<Q',read(renderer,0x7ce00,8))[0]-rbase==0x88248)
check(struct.unpack('<6I',read(renderer,0x88248,24))==(1,0,0,0xb22c8,0x88270,0x88248))
textureTypeName=b'.?AVD3D11Texture@Ogre@@\0'
check(read(renderer,0xb22c8+16,len(textureTypeName))==textureTypeName)
check('if(!ogre||!renderer)return;' in source and 'DWORD(now-state->lastAdmissionTick)<250' in source and 'TryInstallHooks();' in source)
# Native manager signature forwards all stack args, calls Resource.load(false)
# on the returned texture, and returns that same hidden output slot in RAX.
check(read(ogre,0x3798db,15)==bytes.fromhex('f30f108424e0000000f30f11442448'))
check(read(ogre,0x379933,11)==bytes.fromhex('488b0b488b0133d2ff5070'))
check(read(ogre,0x3799b0,3)==bytes.fromhex('488bc3'))
# Generic Resource::load vslot58 invokes the concrete loadImpl while loading;
# its LOADED publication occurs later. Repeated loaded calls skip loadImpl.
check('originalLoad(r,background)' not in source and 'ManagerLoadFn' in source and 'LoadImplFn' in source)
# Native Resource::getSize returns mSize, not pixel buffers or a force-load.
check(syms['?getSize@Resource@Ogre@@UEBA_KXZ']==0x667a0)
check(read(ogre,0x667a0,8)==bytes.fromhex('488b81e0000000c3'))
# Native Texture::calculateSize uses base PixelUtil::getMemorySize, then adds
# CPU/object metadata; getNumFaces virtual+2b0 does not fetch/load buffers.
check(syms['?calculateSize@Texture@Ogre@@MEBA_KXZ']==0x377580)
check(read(ogre,0x3775ce,5)==bytes.fromhex('e89d79eaff'))
check(read(ogre,0x3775dc,6)==bytes.fromhex('ff92b0020000'))
check(read(ogre,0x377600,8)==bytes.fromhex('4a8d842902020000'))
check(syms['?getNumFaces@Texture@Ogre@@UEBA_KXZ']==0x377620)
check(read(ogre,0x377620,32)==bytes.fromhex('4883ec28488b01ff9070010000b901000000ba0600000083f8040f44ca8bc148'))
# These actual native vtable slots are the SDK's passive inherited accessors.
vt=syms['??_7Texture@Ogre@@6B@']
base=struct.unpack_from('<Q',ogre.data,pe+24+24)[0]
for slot,name in [(0xa0,'?getSize@Resource@Ogre@@UEBA_KXZ'),(0x170,'?getTextureType@Texture@Ogre@@UEBA?AW4TextureType@2@XZ'),(0x2b0,'?getNumFaces@Texture@Ogre@@UEBA_KXZ')]:
    check(struct.unpack('<Q',read(ogre,vt+slot,8))[0]-base==syms[name])
# Native TextureManager::createManual passes true as createResource's fifth
# argument (hidden TexturePtr result occupies RDX), before GPU setup.
check(read(ogre,0x379d84,23)==bytes.fromhex('c6442420014c8bcf4d8bc4488d542470498bcd41ff5220'))
# Actual native ResourceManager::unloadUnreferencedResources uses baseline3.
check(struct.unpack('<Q',read(ogre,0x8d3980,8))[0]==3)
check(read(ogre,0x293540,14)==bytes.fromhex('488b43408b4820483b0d32046400'))
# Game ResourceLoader::destroy checks texture controlling refs BEFORE its
# unload(handle) and remove(handle), preserving original native semantics.
check(read(steam,0x44a7b3,27)==bytes.fromhex('498b44d8088b5020488b058ed5df01488b084883c102483bd17752'))
check(read(steam,0x44a7f1,4)==bytes.fromhex('41ff5078'))
check(read(steam,0x44a81c,4)==bytes.fromhex('41ff5048'))
# Ordinary TextureLoadData.getTexture uses createOrRetrieve and external
# loadImage/setToLoaded, bypassing both existing narrow capture hook targets.
for image,site,expected in [(steam,0x44ad74,'41ff9218010000'),(steam,0x44adb8,'ff9038020000'),(steam,0x44adc4,'ff90d0000000'),(renderer,0x47cda,'ff9048020000')]:
    check(read(image,site,len(bytes.fromhex(expected)))==bytes.fromhex(expected))
check(syms['?setToLoaded@Resource@Ogre@@UEAAXXZ']==0x66740)
check(read(ogre,0x66740,11)==bytes.fromhex('c781d000000002000000c3'))
for slot,target in [(0x238,0x47c70),(0x248,0x5830e),(0xd0,0x57738)]:
    check(struct.unpack('<Q',read(renderer,0x7ce08+slot,8))[0]-rbase==target)
check(struct.unpack('<Q',read(ogre,0x6a69a8+0x118,8))[0]-base==0x3795d0)
iat={};irva,isize=renderer.directories[1]
for n in range(isize//20):
    oft,_,_,name,first=struct.unpack_from('<5I',renderer.data,renderer.offset(irva)+20*n)
    if not any((oft,name,first)):break
    dll=renderer.string(name)
    for j in range(32768):
        thunk=struct.unpack_from('<Q',renderer.data,renderer.offset(oft or first)+8*j)[0]
        if not thunk:break
        iat[first+8*j]=dll+'!'+('#'+str(thunk&65535) if thunk>>63 else renderer.string(thunk+2))
for thunk,prefix in [(0x5830e,'OgreMain_x64.dll!?_loadImages@Texture@Ogre@@'),(0x57738,'OgreMain_x64.dll!?setToLoaded@Resource@Ogre@@')]:
    raw=read(renderer,thunk,6)
    check(raw[:2]==b'\xff\x25' and iat[thunk+6+struct.unpack_from('<i',raw,2)[0]].startswith(prefix))
check(read(steam,0x44acd0,32)==bytes.fromhex('40565741544881ec0001000048c78424a0000000feffffff48899c2430010000'))
if os.environ.get('KEO_TEST_GOG_EXE'):
    gog=pem.PE(os.environ['KEO_TEST_GOG_EXE'])
    check(hashlib.sha256(gog.data).hexdigest()=='2defc287c813798b9682b93acf68d340da895cf5164d342145ba0faeb9024681')
    check(read(gog,0x44a8f0,32)==read(steam,0x44acd0,32))
    check(read(gog,0x44a3d3,27)==bytes.fromhex('498b44d8088b5020488b05b6b8df01488b084883c102483bd17752'))
    check(read(gog,0x44a411,4)==read(steam,0x44a7f1,4))
    check(read(gog,0x44a43c,4)==read(steam,0x44a81c,4))
    for at,sz in [(0x44ad74,7),(0x44adb8,6),(0x44adc4,6)]:
        check(read(gog,at-0x3e0,sz)==read(steam,at,sz))
    check('native Steam contract not admitted on GOG' not in source)
    gog_lifetime=re.search(r'lifetime\[\]=\{([^}]+)',source)
    check(gog_lifetime is not None)
    check(read(gog,0x44a3d3,27)==bytes(int(v.strip(),16) for v in gog_lifetime[1].split(',')))
print('texture actual image/export/size/purge contracts checks='+str(checks))

if '--mutants' not in sys.argv:
    sys.exit(0)
out=ROOT/'build/texture_retention_test/mutants';out.mkdir(parents=True,exist_ok=True)
env=os.environ.copy();vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');sdk=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env['INCLUDE']=str(vc/'include')+';'+str(sdk/'Include')
env['LIB']=str(vc/'lib/amd64')+';'+str(sdk/'Lib/x64')
env['PATH']=';'.join([str(vc/'bin/amd64'),str(sdk/'Bin'),str(GAME),env['PATH']])
fixture=(ROOT/'leopard/test_resource_texture_retention.cpp').read_text()
mutations=[
    ('no-retained-native-pin','e.texture=r.staticCast<Ogre::Texture>();','/* controlling TexturePtr omitted */'),
    ('ignore-manager-identity','r.get()!=c.id','false'),
    ('admit-manual','t->isManuallyLoaded()','false'),
    ('ignore-pressure','state->stats.pressure=!resource_gpu::TextureRetentionStandbyAllowed(state->cap);','state->stats.pressure=false;'),
    ('ignore-world','||state->world!=world',''),
    ('ignore-device','||state->device!=device',''),
    ('double-native-load','originalLoadImpl(r);','originalLoadImpl(r);originalLoadImpl(r);'),
    ('ignore-native-mips','*bytes=(std::max)(pixels,accounted);','*bytes=accounted;'),
    ('worker-reads-observer-state','if(!IsMainThread()||!state||!state->on||!r)return;','if(!state||!state->on||!IsMainThread()||!r)return;'),
    ('worker-reads-wrapper-state','if(CaptureEnabled()&&IsMainThread())try{\n        ++state->stats.managerRequests;','if(CaptureEnabled()&&state->on&&IsMainThread())try{\n        ++state->stats.managerRequests;'),
    ('reload-counted-as-reuse','++state->stats.loadCompletions;Observe(r,false);','++state->stats.loadCompletions;Observe(r,true);'),
    ('manager-wrong-float','self,out,name,group,type,mips,gamma,alpha,format,hwGamma);','self,out,name,group,type,mips,2.0f,alpha,format,hwGamma);'),
    ('manager-wrong-bool','self,out,name,group,type,mips,gamma,alpha,format,hwGamma);','self,out,name,group,type,mips,gamma,!alpha,format,hwGamma);'),
    ('manager-wrong-return','return result;','return NULL;'),
    ('disabled-owner-query','if(CaptureEnabled()&&IsMainThread())try{\n        ++state->stats.managerRequests;','if(IsMainThread()&&CaptureEnabled())try{\n        ++state->stats.managerRequests;'),
    ('double-manager-call','Ogre::TexturePtr*result=originalManagerLoad(self,out,name,group,type,mips,gamma,alpha,format,hwGamma);','originalManagerLoad(self,out,name,group,type,mips,gamma,alpha,format,hwGamma);Ogre::TexturePtr*result=originalManagerLoad(self,out,name,group,type,mips,gamma,alpha,format,hwGamma);'),
    ('loader-result-not-observed','        Observe(texture);\n    }catch(...){} // optional completion','        /* missed native result */\n    }catch(...){} // optional completion'),
    ('loader-copies-native-output','Ogre::Texture*texture=output->get();if(!texture)return;','Ogre::TexturePtr copy=*output;Ogre::Texture*texture=copy.get();if(!texture)return;'),
    ('loader-worker-state-read','if(!CaptureEnabled()||!IsMainThread()||!texturePtrOutput)return;','if(!CaptureEnabled()||!state->on||!IsMainThread()||!texturePtrOutput)return;'),
    ('loader-off-owner-query','if(!CaptureEnabled()||!IsMainThread()||!texturePtrOutput)return;','if(!IsMainThread()||!CaptureEnabled()||!texturePtrOutput)return;'),
    ('loader-ignores-size-change','unchanged=Describe(texture,&bytes)&&bytes==held->second.bytes;','unchanged=Describe(texture,&bytes);'),
    ('loader-ignores-new-ineligibility','unchanged=Describe(texture,&bytes)&&bytes==held->second.bytes;','unchanged=(!Describe(texture,&bytes))||bytes==held->second.bytes;'),
    ('loader-observation-exception-escapes','}catch(...){} // optional completion capture cannot replace native success','}catch(...){throw;} // optional completion capture cannot replace native success'),
    ('loader-completion-counter-missing','++state->stats.loaderCompletions;','/* missed actual completion count */'),
]
for name,before,after in mutations:
    check(source.count(before)==1)
    folder=out/name;folder.mkdir(exist_ok=True)
    mutant=folder/'production.cpp';mutant.write_text(source.replace(before,after))
    test=folder/'fixture.cpp';test.write_text(fixture.replace('#include "../src/resources/resource_texture_retention.cpp"','#include "'+mutant.as_posix()+'"'))
    exe=folder/'mutant.exe'
    args=[str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2','/DBOOST_ALL_NO_LIB','/DBOOST_ERROR_CODE_HEADER_ONLY','/D_ITERATOR_DEBUG_LEVEL=0',
          '/I'+str(ROOT/'src'),'/ID:/DEV/Kenshi/deps/KenshiLib_keo/Include','/ID:/DEV/Kenshi/deps/boost_1_60_0',str(test),'/Fo'+str(folder)+'\\','/Fe'+str(exe),
          '/link','/LIBPATH:D:/DEV/Kenshi/deps/KenshiLib_keo/Libraries/ogre','OgreMain_x64.lib']
    build=subprocess.run(args,cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    (folder/'build.log').write_bytes(build.stdout)
    assert build.returncode==0,(name,'build failure is not an accepted RED')
    result=subprocess.run([str(exe)],cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    (folder/'run.log').write_bytes(result.stdout)
    assert result.returncode!=0,(name,'mutant survived')
    print(name+': expected RED')
