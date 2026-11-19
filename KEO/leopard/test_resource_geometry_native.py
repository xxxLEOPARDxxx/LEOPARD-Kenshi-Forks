"""Read-only actual-file native mesh/ownership contracts; game not executed."""
from pathlib import Path
import argparse, hashlib, re, struct

ROOT = Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser()
parser.add_argument('--game',type=Path,default=Path('D:/SteamLibrary/steamapps/common/Kenshi'))
parser.add_argument('--gog-exe',type=Path)
args=parser.parse_args()
GAME = args.game

class Image:
    def __init__(self, path):
        self.data = path.read_bytes()
        pe = struct.unpack_from('<I', self.data, 0x3c)[0]
        assert self.data[pe:pe+4] == b'PE\0\0'
        self.stamp = struct.unpack_from('<I', self.data, pe+8)[0]
        opt = pe+24
        self.size = struct.unpack_from('<I', self.data, opt+56)[0]
        self.base = struct.unpack_from('<Q', self.data, opt+24)[0]
        count = struct.unpack_from('<H', self.data, pe+6)[0]
        sec = opt + struct.unpack_from('<H', self.data, pe+20)[0]
        self.sections = [struct.unpack_from('<8sIIII', self.data, sec+i*40) for i in range(count)]
    def read(self, rva, n):
        for _, size, addr, rawsize, raw in self.sections:
            if addr <= rva and rva+n <= addr+rawsize:
                return self.data[raw+rva-addr:raw+rva-addr+n]
        raise AssertionError('unmapped RVA '+hex(rva))

steam = Image(GAME/'RE_Kenshi/Kenshi_x64.exe')
ogre = Image(GAME/'OgreMain_x64.dll')
assert hashlib.sha256(steam.data).hexdigest() == '504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e'
assert hashlib.sha256(ogre.data).hexdigest() == '7d3888d21398647519f10a89b1f300d8606f761ad159e6021a797b1b9b3df5f6'
source = (ROOT/'src/resources/resource_geometry.cpp').read_text()
guard = re.search(r'bytes\[32\]=\{([^}]+)', source).group(1)
expected = bytes(int(x.strip(), 16) for x in guard.split(','))
assert steam.read(0x449620, 32) == expected
assert steam.stamp == 0x65d604d7 and steam.size == 0x232c000
assert '0x65d604d7' in source and '0x232c000' in source
# Native receives const MeshPtr in RDX, leaves it intact to native scene manager
# createEntity virtual+1B0. This is the consumer used before optional capture.
assert steam.read(0x449655, 20) == bytes.fromhex('488bfa4c8be18b9d90000000488b0a488b01b201')
assert steam.read(0x44967d, 9) == bytes.fromhex('488bd7ff90b0010000')
# Real manager vtable+68 and native control-block useCount comparison3.
assert struct.unpack('<Q', ogre.read(0x68e748+0x68,8))[0]-ogre.base == 0x2934f0
assert ogre.read(0x293540, 14) == bytes.fromhex('488b43408b4820483b0d32046400')
assert struct.unpack('<Q', ogre.read(0x8d3980,8))[0] == 3
# Real MeshManager remove/getByHandle/addImpl/removeImpl slots used by the
# orphan fixture. Native remove only resolves, unregisters, then releases its
# temporary reference; externally owned meshes can stay loaded (fixture proof).
for slot, target in ((0x78,0x2937a0),(0xa8,0x293ce0),(0x100,0x292460),(0x108,0x292fc0)):
    assert struct.unpack('<Q',ogre.read(0x68e748+slot,8))[0]-ogre.base==target
assert ogre.read(0x2937ce,17)==bytes.fromhex('488b03488d542428488bcbff9008010000')
# The manual-LOD accessor actually loads absent LODs. Collector must reject
# these without calling it; this prevents accidental owner-thread cold loads.
assert ogre.read(0x1a733d,5) == bytes.fromhex('e82e740100')
assert 'm->hasManualLodLevel()' in source and 'm->getLodLevel(' not in source
# Final morph ownership/registration proof from the pinned native files.
# Relative IAT calls identify the actual passive getter/clone/pose-removal
# exports, rather than relying only on names in an SDK header or a report.
def imported_pointer(image,iat):
    name=struct.unpack('<Q',image.read(iat,8))[0]+2
    data=bytearray()
    while image.read(name+len(data),1)!=b'\0':data+=image.read(name+len(data),1)
    return data.decode('ascii')
def imported_call(image,rva):
    code=image.read(rva,6)
    assert code[:2]==b'\xff\x15'
    return imported_pointer(image,rva+6+struct.unpack('<i',code[2:])[0])
assert imported_call(steam,0x71ac9).startswith('?getMesh@Entity@Ogre@@')
assert imported_call(steam,0x71ae1).startswith('?clone@Mesh@Ogre@@')
assert imported_call(steam,0x71cdc)=='?removeAllPoses@Mesh@Ogre@@QEAAXXZ'
assert ogre.read(0xc09b0,8)==bytes.fromhex('488d8178010000c3') # borrowed MeshPtr ref, no load/refcopy
assert ogre.read(0x1a3fd8,5)==bytes.fromhex('e893a90100') # clone -> createManual 1BE970
assert ogre.read(0x1a4350,53)==bytes.fromhex('488b4b40488d91d0000000448b4108e88c2fe7ff488bf8488b53404881c2d0000000498b4d004881c118030000e84e860000488938')
# Switch destroys the old Entity, releases its owned16 temporary, returns new
# R12 via RAX. Production keeps this raw ABI and observes only that return.
assert steam.read(0x71a1f,5)==bytes.fromhex('e85f2ffbff')
assert steam.read(0x71a25,59)==bytes.fromhex('48833f00742a488b4f0883c8fff00fc14120ffc8751a488b4f084885c97411488b0133d2ff10488b4f08ff15b35d1d0248c7070000000048c74708')
assert steam.read(0x71a64,3)==bytes.fromhex('498bc4')
assert steam.read(0x71d07,8)==bytes.fromhex('e888c9fcff488bd8')
# Destroy's real native useCount condition: remove only at SYSTEM+1; an
# additional admitted MeshPtr pin defeats equality. No hardcoded game policy
# value is introduced by production capture.
assert steam.read(0x44a5e7,26)==bytes.fromhex('488b4424408b5020488b055ad7df01488b0848ffc1483bd1754b')
assert imported_pointer(steam,0x44a5f6+struct.unpack('<i',steam.read(0x44a5f2,4))[0])=='?RESOURCE_SYSTEM_NUM_REFERENCE_COUNTS@ResourceGroupManager@Ogre@@2_KA'
if args.gog_exe:
    gog=Image(args.gog_exe)
    assert hashlib.sha256(gog.data).hexdigest()=='2defc287c813798b9682b93acf68d340da895cf5164d342145ba0faeb9024681'
    assert (gog.stamp,gog.size)==(0x65d60519,0x232a000)
    assert gog.read(0x449240,32)==expected
    assert gog.read(0x449275,20)==steam.read(0x449655,20)
    assert gog.read(0x44929d,9)==steam.read(0x44967d,9)
    assert gog.read(0x44a207,11)==steam.read(0x44a5e7,11)
    assert gog.read(0x44a216,11)==steam.read(0x44a5f6,11)
    assert imported_pointer(gog,0x44a216+struct.unpack('<i',gog.read(0x44a212,4))[0])=='?RESOURCE_SYSTEM_NUM_REFERENCE_COUNTS@ResourceGroupManager@Ogre@@2_KA'
    assert 'gog?0x449240:0x449620' in source, 'GOG mesh requests still lack a runtime profile'
probe=(ROOT/'src/resources/resource_load_probe.cpp').read_text()
capture=probe[probe.index('static void* SwitchBodyMesh('):probe.index('static void SetBodyEntity(')]
assert capture.count('CompleteFinalMeshCapture(result)')==2
assert 'CompleteFinalMeshCapture(entity)' not in capture and 'CompleteFinalMeshCapture(ownedMesh)' not in capture
assert source.index('if(!IsMainThread()||!state||!state->on||!entity)return;',source.index('ObserveFinalEntityImpl'))<source.index('Observe(entity->getMesh());')
print('native Steam request/Ogre SharedPtr-purge/mesh-manager orphan/manual-LOD/final-morph ownership contracts passed')
if args.gog_exe:print('native GOG mesh request, ABI and ownership contracts passed')
