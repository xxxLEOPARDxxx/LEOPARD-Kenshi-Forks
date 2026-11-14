"""Read-only actual-file native mesh/ownership contracts; game not executed."""
from pathlib import Path
import hashlib, re, struct

ROOT = Path(__file__).resolve().parents[1]
GAME = ROOT.parent.parent

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
# The manual-LOD accessor actually loads absent LODs. Collector must reject
# these without calling it; this prevents accidental owner-thread cold loads.
assert ogre.read(0x1a733d,5) == bytes.fromhex('e82e740100')
assert 'm->hasManualLodLevel()' in source and 'm->getLodLevel(' not in source
print('native Steam request/Ogre SharedPtr-purge/manual-LOD contracts passed')
