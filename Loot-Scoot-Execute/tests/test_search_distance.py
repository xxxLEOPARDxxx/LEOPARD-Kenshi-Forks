"""Compile the production search-radius conversion, not a duplicated formula."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'Loot-Scoot-Execute.cpp').read_text(encoding='utf-8-sig')
start = source.index('static float ComputeExecuteAllRadiusUnits()\n{')
end = source.index('\nstatic bool IsExecuteAllEnabled()', start)
function = source[start:end]
vc = Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC')
sdk = Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env = os.environ.copy()
env['PATH'] = str(vc / 'bin/amd64') + ';' + env['PATH']
env['INCLUDE'] = str(vc / 'include') + ';' + str(sdk / 'Include')
env['LIB'] = str(vc / 'lib/amd64') + ';' + str(sdk / 'Lib/x64')
fixture = r'''
#include <cstdio>
struct Config { int executeAllRadiusUnits; } g_config;
const int kExecuteAllRadiusMinUnits=1, kExecuteAllRadiusMaxUnits=200;
// @PRODUCTION@
int main() {
    const int input[]={-10,0,1,2,10,100,200,201,2147483647};
    const float expected[]={10,10,10,20,100,1000,2000,2000,2000};
    for (int i=0;i<9;++i) {
        g_config.executeAllRadiusUnits=input[i];
        if (ComputeExecuteAllRadiusUnits()!=expected[i]) {
            std::printf("FAIL configured metres=%d, native=%g, expected=%g\n",
                input[i],ComputeExecuteAllRadiusUnits(),expected[i]);
            return 1;
        }
    }
    std::puts("9 production search-radius metre checks passed");
}
'''.replace('// @PRODUCTION@', function)
with tempfile.TemporaryDirectory(prefix='lse-distance-') as temporary:
    directory = Path(temporary)
    cpp = directory / 'distance.cpp'
    cpp.write_text(fixture, encoding='utf-8')
    exe = directory / 'distance.exe'
    subprocess.run([str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/O2','/MD',str(cpp),
                    '/Fe'+str(exe),'/Fo'+str(directory/'distance.obj')],
                   cwd=directory, env=env, check=True)
    subprocess.run([str(exe)], check=True)
