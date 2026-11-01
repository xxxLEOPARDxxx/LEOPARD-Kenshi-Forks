"""Compile both real texture loaders with narrow MyGUI boundary doubles.

The regression is observable: probing absent aliases must not call the texture
loader (which emits MyGUI errors), and explicit user paths keep their priority.
Run with bundled Python or python; needs the project's VC10/Windows SDK 7.1.
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
VC = Path(r"C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC")
SDK = Path(r"C:\Program Files\Microsoft SDKs\Windows\v7.1")


def function(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


BOUNDARY = r'''
#include <string>
#include <vector>
#include <map>
#include <set>
#include <deque>
#include <cstring>
#include <cstdio>
#include <stdexcept>
#include <sstream>
int failures=0, checks=0, errors=0, missingLoads=0, probes=0;
std::set<std::string> files, corrupt, manual, probeFailures;
std::vector<std::string> applied;
void ErrorLog(const char*) { ++errors; }
#define CHECK(x) do {++checks; if(!(x)){++failures; printf("FAIL line %d: %s\n",__LINE__,#x);}} while(0)
namespace MyGUI {
struct IntSize {int width,height; IntSize(int w=0,int h=0):width(w),height(h){} };
struct IntCoord {IntCoord(int,int,int,int){} };
class DataManager {
public:
 static DataManager* current;
 static DataManager* getInstancePtr(){return current;}
 bool isDataExist(const std::string& s){++probes; if(probeFailures.count(s))throw std::runtime_error("probe"); return files.count(s)!=0;}
};
DataManager* DataManager::current=0;
class RenderManager {
public:
 static RenderManager* current;
 static RenderManager* getInstancePtr(){return current;}
 void* getTexture(const std::string& s){return manual.count(s)?this:0;}
};
RenderManager* RenderManager::current=0;
class ImageBox {
public:
 std::string texture;
 void setImageTexture(const std::string& s){
  applied.push_back(s);
  if(!files.count(s) && !manual.count(s)){++missingLoads; throw std::runtime_error("missing");}
  if(corrupt.count(s)){++errors; throw std::runtime_error("decode");}
  texture=s;
 }
 IntSize getImageSize(){return IntSize(64,64);}
 void setImageCoord(const IntCoord&){}
 void setImageTile(const IntSize&){}
};
}
struct Perf {int imageTextureCandidateAttempts,imageTextureApplySuccesses,imageTextureApplyFailures;
 Perf():imageTextureCandidateAttempts(0),imageTextureApplySuccesses(0),imageTextureApplyFailures(0){} } g_overlayPerfStats;
struct Config {bool debugLogTextureInfo; Config():debugLogTextureInfo(false){} };
struct RuntimeStateView {Config config; std::vector<std::string> iconTextureWarnLogs,iconTextureOkLogs;};
bool gMarkerDiagnosticsEnabled=false;
unsigned gMarkerDiagTextureApplyCalls=0;
void LogIconTextureOnce(RuntimeStateView&, const char*,std::vector<std::string>& sink,const std::string&,const char* name,bool warn){
 if(!warn)return;
 for(size_t i=0;i<sink.size();++i)if(sink[i]==name)return;
 sink.push_back(name); ++errors;
}
'''

CASES = r'''
int main(){
 MyGUI::DataManager dm; MyGUI::DataManager::current=&dm;
 MyGUI::RenderManager rm; MyGUI::RenderManager::current=&rm;
 files.insert("death.png"); files.insert(FALLBACK);
 MyGUI::ImageBox icon;
 CHECK(LOAD("gui/gfx/death.png"));
 CHECK(icon.texture=="death.png"); CHECK(missingLoads==0);
 int prior=probes;
 CHECK(LOAD("gui/gfx/death.png")); CHECK(probes==prior);

 // A deliberately configured path wins over a same-basename resource.
 files.insert("custom/collision.png"); files.insert("collision.png");
 CHECK(LOAD("custom/collision.png")); CHECK(icon.texture=="custom/collision.png");
 files.insert("custom\\back.png");
 CHECK(LOAD("custom/back.png")); CHECK(icon.texture=="custom\\back.png");
 files.insert("C:/custom/absolute.png");
 CHECK(LOAD("C:/custom/absolute.png")); CHECK(icon.texture=="C:/custom/absolute.png");
 files.insert("mods/Other/gui/icon.png");
 CHECK(LOAD("mods/Other/gui/icon.png")); CHECK(icon.texture=="mods/Other/gui/icon.png");

 int before=errors;
 CHECK(LOAD_MISSING("gui/gfx/absent.png")); CHECK(icon.texture==FALLBACK);
 CHECK(errors==before+1); CHECK(missingLoads==0);
 prior=probes;
 CHECK(LOAD_MISSING("gui/gfx/absent.png")); CHECK(errors==before+1); CHECK(probes==prior);

 // A present but corrupt texture still reaches the real loader and reports its error.
 files.insert("custom/bad.png"); corrupt.insert("custom/bad.png"); files.insert("bad.png");
 before=errors;
 CHECK(LOAD("custom/bad.png")); CHECK(icon.texture=="bad.png"); CHECK(errors>before);
 CHECK(missingLoads==0);

 // GUI startup is not a permanently missing resource.
 MyGUI::DataManager::current=0;
 before=errors; prior=probes;
 CHECK(!LOAD("later.png")); CHECK(errors==before); CHECK(probes==prior);
 files.insert("later.png"); MyGUI::DataManager::current=&dm;
 CHECK(LOAD("later.png")); CHECK(icon.texture=="later.png");

 // A new DataManager starts a new resource catalog; complete failure stays visible.
 MyGUI::DataManager replacement; MyGUI::DataManager::current=&replacement;
 files.erase(FALLBACK); before=errors;
 CHECK(!LOAD("totally-missing.png")); CHECK(errors==before+2); CHECK(missingLoads==0);
 prior=probes;
 CHECK(!LOAD("totally-missing.png")); CHECK(errors==before+2); CHECK(probes==prior);

 // Exercise the actual bounded cache, including negative entries and FIFO eviction.
 vital_texture::PreflightCache cache;
 prior=probes;
 CHECK(!cache.Exists("not-installed.png")); CHECK(!cache.Exists("not-installed.png"));
 CHECK(probes==prior+1);
 CHECK(cache.ReportMissingOnce("not-installed.png")); CHECK(!cache.ReportMissingOnce("not-installed.png"));
 for(int i=0;i<300;++i){std::stringstream n; n<<"absent-"<<i; CHECK(!cache.Exists(n.str()));}
 prior=probes; CHECK(!cache.Exists("absent-299")); CHECK(probes==prior);
 CHECK(!cache.Exists("absent-0")); CHECK(probes==prior+1);
 MyGUI::DataManager::current=&dm; prior=probes;
 CHECK(!cache.Exists("absent-299")); CHECK(probes==prior+1);
 CHECK(!cache.Exists("runtime.png")); manual.insert("runtime.png");
 CHECK(cache.Exists("runtime.png")); CHECK(LOAD("runtime.png")); CHECK(icon.texture=="runtime.png");
 files.insert("probe-throws.png"); probeFailures.insert("probe-throws.png");
 CHECK(LOAD("probe-throws.png")); CHECK(icon.texture=="probe-throws.png");
 printf("%s: %d checks, %d failures\n",LABEL,checks,failures);
 return failures ? 1 : 0;
}
'''


def main():
    env = os.environ.copy()
    env["INCLUDE"] = f"{VC / 'include'};{SDK / 'Include'}"
    env["LIB"] = f"{VC / 'lib/amd64'};{SDK / 'Lib/x64'}"
    env["PATH"] = f"{VC / 'bin/amd64'};{env['PATH']}"
    failed = False
    with tempfile.TemporaryDirectory(prefix="vital-texture-test-") as temp:
        folder = Path(temp)
        for plugin, filename, signature in [
            ("Vital-Read", "vr_marker_ui.cpp", "bool EnsureTextureApplied("),
            ("Vital-Sense", "vs_marker_render.cpp", "bool SetKoMarkerIconTexture("),
        ]:
            src = ROOT / plugin / "src"
            source = (src / filename).read_text(encoding="utf-8-sig")
            helper = src / "TexturePreflight.h"
            helper_text = helper.read_text(encoding="utf-8") if helper.exists() else ""
            helper_text = "\n".join(line for line in helper_text.splitlines() if not line.startswith("#include") and line != "#pragma once")
            if plugin == "Vital-Read":
                macros = '#define FALLBACK "default_icon.png"\n#define LOAD(s) EnsureTextureApplied(&icon,"Vital-Read",s,64)\n#define LOAD_MISSING(s) LOAD(s)\n'
            else:
                macros = '#define FALLBACK "Kenshi_UI.png"\nRuntimeStateView state;\n#define LOAD(s) SetKoMarkerIconTexture(state,"Vital-Sense",&icon,s)\n#define LOAD_MISSING(s) (!LOAD(s))\n'
            code = (BOUNDARY + helper_text + "\n" + function(source, "bool StringListContains(")
                    + "\n" + function(source, signature) + "\n" + macros
                    + f'#define LABEL "{plugin}"\n' + CASES)
            cpp = folder / (plugin + ".cpp")
            exe = folder / (plugin + ".exe")
            cpp.write_text(code, encoding="utf-8")
            compiled = subprocess.run([str(VC / "bin/amd64/cl.exe"), "/nologo", "/EHsc", "/MD", "/W3", str(cpp), "/Fe" + str(exe)], cwd=folder, env=env, capture_output=True, text=True)
            if compiled.returncode:
                print(compiled.stdout, compiled.stderr)
                raise SystemExit(compiled.returncode)
            result = subprocess.run([str(exe)], cwd=folder, env=env, capture_output=True, text=True)
            print(result.stdout, end="")
            failed |= bool(result.returncode)
    raise SystemExit(1 if failed else 0)


if __name__ == "__main__":
    main()
