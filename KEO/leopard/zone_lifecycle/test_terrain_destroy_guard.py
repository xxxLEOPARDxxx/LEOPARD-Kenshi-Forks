"""Execute the actual terrain teardown branch island in a private buffer."""
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
source = (root / 'src/zone/handoff/zone_lifecycle_hooks.cpp').read_text(encoding='utf-8')
stub = source[source.index('bool WriteTerrainBranch('):source.index('void InstallTerrainDestroyGuard(')]
fixture = r'''
#include <windows.h>
#include <stdint.h>
#include <cstdio>
#include <cstring>
// Incoming RCX is the owned pointer. Put it in RBP as the native prologue
// does. RDX begins as an unrelated pointer to catch incorrect cleanup ABI.
int main() {
    unsigned char* page = static_cast<unsigned char*>(VirtualAlloc(
        NULL, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!page) return 2;
    const unsigned char entry[] = {0x53,0x55,0x48,0x8b,0xe9,0xe9,0,0,0,0};
    const unsigned char normal[] = {0x48,0x8d,0x43,0x01,0x48,0x89,0x01,0x5d,0x5b,0xc3}; // *RCX = manager + 1
    const unsigned char cleanup[] = {0x48,0xc7,0x02,0,0,0,0,0x5d,0x5b,0xc3}; // *RDX = NULL
    memcpy(page, entry, sizeof(entry));
    memcpy(page + 128, normal, sizeof(normal));
    memcpy(page + 144, cleanup, sizeof(cleanup));
    if (!WriteTerrainBranch(page + 6, uintptr_t(page + 64)) ||
        !EncodeTerrainDestroyStub(page + 64, uintptr_t(page + 192),
                                  uintptr_t(page + 128), uintptr_t(page + 144))) return 2;
    DWORD old = 0;
    if (!VirtualProtect(page, 4096, PAGE_EXECUTE_READWRITE, &old) ||
        !FlushInstructionCache(GetCurrentProcess(), page, 4096)) return 2;
    typedef void (*Callback)(uintptr_t*, uintptr_t*);
    Callback run = reinterpret_cast<Callback>(page);
    uintptr_t* manager = reinterpret_cast<uintptr_t*>(page + 192);
    uintptr_t material = 17, other = 31;
    *manager = 43;
    run(&material, &other);
    if (material != 44 || other != 31) { puts("FAIL native continuation"); return 1; }
    *manager = 0;
    material = 17;
    run(&material, &other);
    if (material != 0 || other != 31) { puts("FAIL release pointer"); return 1; }
    VirtualFree(page, 0, MEM_RELEASE);
    puts("Terrain material guard: normal and native release branch passed");
    return 0;
}
'''

out = root / 'build/terrain-destroy-guard'
out.mkdir(parents=True, exist_ok=True)
vc = Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC')
sdk = Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env = dict(os.environ, INCLUDE=f'{vc}/include;{sdk}/Include', LIB=f'{vc}/lib/amd64;{sdk}/Lib/x64')
before = '#include <stdint.h>\n#include <cstring>\n'
no_guard = stub.replace('0x0f,0x85,0,0,0,0', '0x90,0xe9,0,0,0,0')
assert no_guard != stub
for name, code, expected in [('normal', stub, 0), ('no-guard', no_guard, 1)]:
    unit, exe = out / (name + '.cpp'), out / (name + '.exe')
    unit.write_text(before + code + fixture, encoding='utf-8')
    subprocess.run([str(vc / 'bin/amd64/cl.exe'), '/nologo', '/EHsc', '/O2', str(unit),
                    f'/Fo{out / (name + ".obj")}', f'/Fe{exe}'], env=env, check=True, cwd=out)
    result = subprocess.run([str(exe)], env=env)
    assert result.returncode == expected, (name, result.returncode)
