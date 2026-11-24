"""Exercise the furnace eligibility wrapper and its actual Steam/GOG call sites."""
from hashlib import sha256
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
from tempfile import TemporaryDirectory
from zipfile import ZipFile

sys.path.insert(0, 'D:/DEV/Kenshi/tools/_native_python')
import pefile

root = Path(__file__).resolve().parents[1]
game = Path('D:/SteamLibrary/steamapps/common/Kenshi')
source = (root / 'src/inventory/dialogue_item_function.cpp').read_text()
profile = (root / 'src/game/gog_profile_data.inc').read_text()
guard_rva, lead_rva, call_rva = (0x5a3d12, 0x5a3d1d, 0x5a3d36)
for name, rva in [('kFurnaceGuardRva', guard_rva), ('kFurnaceLeadRva', lead_rva),
                  ('kFurnaceCallRva', call_rva)]:
    assert int(re.search(rf'{name}\s*=\s*(0x[\da-fA-F]+)', source)[1], 16) == rva

def bytes_in(text):
    return bytes(int(h, 16) for h in re.findall(r'0x([\da-fA-F]{2})\b', text))

guard, lead = [bytes_in(re.search(rf'{name}\[[^\]]+\]\s*=\s*\{{([^}}]+)\}}', source)[1])
               for name in ('kFurnaceGuard', 'kFurnaceLead')]
steam = pefile.PE(str(game / 'RE_Kenshi/Kenshi_x64.exe'), fast_load=True)
assert sha256(steam.__data__).hexdigest() == '504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e'
assert steam.get_data(guard_rva, len(guard)) == guard
assert steam.get_data(lead_rva, len(lead)) == lead
with ZipFile(game / 'DropMeFiles_VfFSw.zip') as archive:
    for member in archive.infolist():
        data = archive.read(member)
        if sha256(data).hexdigest() == '2defc287c813798b9682b93acf68d340da895cf5164d342145ba0faeb9024681':
            gog = pefile.PE(data=data, fast_load=True)
            break
    else:
        raise AssertionError('Supported GOG image missing')
for address, count in [(guard_rva, len(guard)), (lead_rva, len(lead))]:
    mapped = re.search(rf'\{{0x{address:X}u,0x([\da-fA-F]+)u\}}', profile)
    window = re.search(rf'\{{0x{address:X}u,\{{([^}}]+)\}}', profile)
    assert gog.get_data(int(mapped[1], 16), count) == bytes_in(window[1])[:count]
for version, address, target in [(steam, call_rva, 0x4b812), (gog, 0x5a4046, 0x4b88a)]:
    opcode, rel32 = struct.unpack('<Bi', version.get_data(address, 5))
    assert opcode == 0xe8 and address + 5 + rel32 == target

begin = source.index('static bool __fastcall KEO_FurnaceHasItemFunction')
wrapper = source[begin:source.index('\n}', begin) + 2]
fixture = r'''
#include <cassert>
static const int kSeveredLimb = 17;
struct { bool backpackFixesEnabled; } g_inventoryCfg;
static int mainInv, bagInv, actor, visits, queries;
static bool mainHas, bagHas, wears;
bool fn_hasItemFunction(const void* inv, int type)
{
    ++queries;
    return type == 17 && (inv == &mainInv ? mainHas : bagHas);
}
void* WornBackpackInventory(void* who)
{
    assert(who == &actor);
    ++visits;
    return wears ? &bagInv : 0;
}
''' + wrapper + r'''
int main()
{
    void* ai[0x2f8 / sizeof(void*) + 1] = {};
    ai[0x2f8 / sizeof(void*)] = &actor;
    wears = bagHas = true;
    assert(!KEO_FurnaceHasItemFunction(&mainInv, 17, ai) && visits == 0 && queries == 1);
    g_inventoryCfg.backpackFixesEnabled = true;
    assert(KEO_FurnaceHasItemFunction(&mainInv, 17, ai) && visits == 1 && queries == 3);
    assert(!KEO_FurnaceHasItemFunction(&mainInv, 14, ai) && visits == 1 && queries == 4);
    assert(!KEO_FurnaceHasItemFunction(&mainInv, 17, 0) && visits == 1 && queries == 5);
    mainHas = true;
    assert(KEO_FurnaceHasItemFunction(&mainInv, 17, ai) && visits == 1 && queries == 6);
    mainHas = wears = false;
    assert(!KEO_FurnaceHasItemFunction(&mainInv, 17, ai) && visits == 2 && queries == 7);
    wears = true;
    bagHas = false;
    assert(!KEO_FurnaceHasItemFunction(&mainInv, 17, ai) && visits == 3 && queries == 9);
}
'''
vc = Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC')
sdk = Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env = dict(os.environ, PATH=f'{vc}/bin/amd64;{os.environ["PATH"]}',
           INCLUDE=f'{vc}/include;{sdk}/Include', LIB=f'{vc}/lib/amd64;{sdk}/Lib/x64')
with TemporaryDirectory() as temp:
    Path(temp, 'test.cpp').write_text(fixture)
    subprocess.run([str(vc / 'bin/amd64/cl.exe'), '/nologo', '/EHsc', 'test.cpp'], cwd=temp, env=env, check=True)
    subprocess.run([str(Path(temp, 'test.exe'))], check=True)
print('Furnace backpack eligibility and both native call sites pass')
