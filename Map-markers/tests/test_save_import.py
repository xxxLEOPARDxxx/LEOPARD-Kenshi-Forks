"""Production bookmark persistence and lifecycle wrappers with owned save files.

The SaveFileSystem seam models only its verified file registration/copy boundary;
no game process, user save or UI is touched.
"""
from pathlib import Path
import os
import subprocess
import uuid

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'tests/_generated/save-import'
OUT.mkdir(parents=True, exist_ok=True)
main = (ROOT/'Map-markers.cpp').read_text(encoding='utf-8-sig')
data = (ROOT/'src/MapMarkersData.cpp').read_text(encoding='utf-8-sig')


def body(text, signature):
    start = text.index(signature)
    while ';' in text[start:text.index('{', start)]:
        start = text.index(signature, start+len(signature))
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


pure = '\n'.join(body(data, s) for s in [
    'std::string ToLowerAscii(', 'float ClampFloat(', 'std::string SanitizeMarkerLabel(',
    'const char* MarkerTypeToJsonValue(', 'MarkerType MarkerTypeFromString(',
    'std::string JsonEscapeString(', 'std::string JoinWindowsPath(',
    'std::string NormalizePathForComparison(', 'bool PathsEqualIgnoreCase(',
    'bool ExtractJsonFloatField(', 'bool ExtractJsonIntField(',
    'bool ExtractJsonStringField(', 'bool TryExtractJsonArrayContents('])
actual = '\n'.join(body(main, s) for s in [
    'std::string GetActiveSaveDirectory()', 'std::string GetMarkerPersistencePath()',
    'bool SaveMarkersToPath(', 'void SaveMarkersForActiveSave(',
    'void ApplyMarkerStateForLoadedSave(', 'void ClearMarkerStateForLoadedSave()',
    'MarkerPersistenceLoadAttempt LoadMarkersFromPersistencePath(',
    'void LoadMarkersForActiveSave()'])
if 'int SaveManager_importGame_hook(' in main:
    actual += '\n' + '\n'.join(body(main, s) for s in [
        'void RestoreMarkersAfterImport(', 'bool SaveFileSystem_saveGame_hook(',
        'void SaveFileSystem_loadGame_hook(', 'void SaveFileSystem_newGame_hook(',
        'int SaveManager_importGame_hook('])
    invoke = 'return SaveManager_importGame_hook(&manager,root,name,flags);'
else:
    actual += '\n' + body(main, 'void SaveManager_import_hook(')
    # Old request hook has no bookmark transfer when queued work executes.
    actual += '''\nbool SaveFileSystem_saveGame_hook(SaveFileSystem*s,const std::string&p){return OriginalSave(s,p);}
void SaveFileSystem_loadGame_hook(SaveFileSystem*s,const std::string&p){OriginalLoad(s,p);LoadMarkersForActiveSave();}
void SaveFileSystem_newGame_hook(SaveFileSystem*s){OriginalNew(s);LoadMarkersForActiveSave();}\n'''
    invoke = 'SaveInfo info;info.location=root;info.name=name;SaveManager_import_hook(&manager,info,flags);return lastResult;'

fixture = r'''
#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <cctype>
#include <stdexcept>
#include "MapMarkersInternal.h"
unsigned checks=0,errors=0;int nativeCalls=0,mode=0,lastResult=0;std::string root;
void C(bool ok,const char*n){++checks;if(!ok){std::cout<<"FAIL "<<n<<"\n";std::exit(1);}}
std::string Text(const std::string&p){std::ifstream f(p.c_str(),std::ios::binary);std::stringstream s;s<<f.rdbuf();return s.str();}
std::vector<MarkerState> g_markers;int g_selectedMarkerId=0;std::string g_lastMarkerRenderSignature;
bool g_pendingSaveTransitionActive=false;std::string g_pendingSaveTransitionDestinationPath;
const char*kMarkerPersistenceFileName="Map-markers.json";
void RefreshNextMarkerId(){}void ResetMarkersForActiveSave(){g_markers.clear();g_selectedMarkerId=0;}
void ClearPendingSaveTransition(const char*){g_pendingSaveTransitionActive=false;}
void LogProbeLine(const std::string&){}void ErrorLog(const char*){++errors;}
void ErrorLog(const std::string&){++errors;}
@PURE@
struct SaveInfo{std::string name,location;};struct SaveManager{}manager;
class SaveFileSystem {
public:
 std::string active,temp;std::map<std::string,std::string> files;int generation;
 SaveFileSystem():generation(0){}
 static SaveFileSystem*getSingleton();
 const std::string&getActiveSave()const{return active;}
 bool fileExists(const std::string&name)const{return files.find(name)!=files.end()&&GetFileAttributesA(files.find(name)->second.c_str())!=INVALID_FILE_ATTRIBUTES;}
 std::string readFile(const std::string&name)const{return fileExists(name)?files.find(name)->second:std::string();}
 std::string writeFile(const std::string&name){std::string p=JoinWindowsPath(temp,name.c_str());if(fileExists(name)&&p!=files[name])C(CopyFileA(files[name].c_str(),p.c_str(),FALSE)!=0,"owned copy-on-write source");files[name]=p;return p;}
 void Fresh(){++generation;std::ostringstream n;n<<root<<"/temp"<<generation;temp=n.str();C(CreateDirectoryA(temp.c_str(),NULL)!=0,"new owned temporary directory");files.clear();active.clear();}
}fs;
SaveFileSystem*SaveFileSystem::getSingleton(){return &fs;}
void LoadMarkersForActiveSave();
bool OriginalSave(SaveFileSystem*s,const std::string&p){++nativeCalls;if(mode==21)return false;CreateDirectoryA(p.c_str(),NULL);for(std::map<std::string,std::string>::iterator i=s->files.begin();i!=s->files.end();++i)C(CopyFileA(i->second.c_str(),JoinWindowsPath(p,i->first.c_str()).c_str(),FALSE)!=0,"registered sidecar copied by native save seam");s->active=p;return true;}
void OriginalLoad(SaveFileSystem*s,const std::string&p){++nativeCalls;s->Fresh();s->active=p;std::string f=JoinWindowsPath(p,kMarkerPersistenceFileName);if(GetFileAttributesA(f.c_str())!=INVALID_FILE_ATTRIBUTES)s->files[kMarkerPersistenceFileName]=f;}
void OriginalNew(SaveFileSystem*s){++nativeCalls;s->Fresh();}
bool SaveFileSystem_saveGame_hook(SaveFileSystem*,const std::string&);
void SaveFileSystem_loadGame_hook(SaveFileSystem*,const std::string&);
void SaveFileSystem_newGame_hook(SaveFileSystem*);
std::vector<MarkerState> One(const char*label){MarkerState m;m.id=1;m.normalizedX=.25f;m.normalizedY=.75f;m.type=MarkerType_Stash;m.label=label;return std::vector<MarkerState>(1,m);}
int OriginalImportGame(SaveManager*,const std::string&location,const std::string&name,int flags){++nativeCalls;C(location==root&&flags==34,"selected import source and flags forwarded unchanged");if(mode==11)throw std::runtime_error("owned import failure");if(mode==1||mode==2)return mode;if(mode==5||mode==7){SaveFileSystem_loadGame_hook(&fs,root+"/B");return mode==5?1:0;}SaveFileSystem_newGame_hook(&fs);if(mode==6)return 2;if(mode==12)throw std::runtime_error("owned post-reset failure");if(mode==8)const_cast<std::string&>(name)="B";if(mode==3)SaveMarkersFile(fs.writeFile(kMarkerPersistenceFileName),One("DEST"));if(mode==4){std::ofstream f(fs.writeFile(kMarkerPersistenceFileName).c_str());f<<"damaged destination";}return 0;}
void OriginalRequest(SaveManager*s,const SaveInfo&i,int f){lastResult=OriginalImportGame(s,i.location,i.name,f);LoadMarkersForActiveSave();}
bool(*SaveFileSystem_saveGame_orig)(SaveFileSystem*,const std::string&)=OriginalSave;
void(*SaveFileSystem_loadGame_orig)(SaveFileSystem*,const std::string&)=OriginalLoad;
void(*SaveFileSystem_newGame_orig)(SaveFileSystem*)=OriginalNew;
int(*SaveManager_importGame_orig)(SaveManager*,const std::string&,const std::string&,int)=OriginalImportGame;
void(*SaveManager_import_orig)(SaveManager*,const SaveInfo&,int)=OriginalRequest;
@ACTUAL@
int Import(const std::string&name,int flags=34){@INVOKE@}
bool Is(const char*s){return g_markers.size()==1&&g_markers[0].label==s;}
void Load(const char*n){SaveFileSystem_loadGame_hook(&fs,root+"/"+n);}
void Save(const char*n){C(SaveFileSystem_saveGame_hook(&fs,root+"/"+n),"accepted native save");}
int main(int argc,char**argv){C(argc==2,"owned fixture root argument");root=argv[1];fs.Fresh();
 for(int i=0;i<3;++i){const char*name=i==0?"A":i==1?"B":"EMPTY";std::string dir=root+"/"+name;C(CreateDirectoryA(dir.c_str(),NULL)!=0,"owned source slot");C(SaveMarkersFile(JoinWindowsPath(dir,kMarkerPersistenceFileName),i<2?One(name):std::vector<MarkerState>()),"real production JSON source write");}
 const std::string originalA=Text(root+"/A/Map-markers.json");Load("B");C(Is("B"),"normal load reads only selected B");
 C(Import("A")==0,"native import success");C(Is("A"),"import selected A retains A bookmarks while active world was B");C(Text(root+"/A/Map-markers.json")==originalA,"import never changes original source sidecar");
 Save("IMPORTED");Load("IMPORTED");C(Is("A"),"first save after import copies registered bookmark data");Save("RENAMED");Load("RENAMED");C(Is("A"),"Save As and reload retain same world bookmarks");
 Load("B");mode=1;C(Import("A")==1&&Is("B"),"failed native import never applies staged source");mode=2;C(Import("A")==2&&Is("B"),"retry never publishes staged source");mode=11;try{Import("A");C(false,"exception propagated");}catch(const std::runtime_error&){}C(Is("B"),"throw has no staged carry state");
 mode=5;C(Import("A")==1&&Is("B"),"nested load plus failed import keeps actual loaded world only");mode=6;C(Import("A")==2&&g_markers.empty(),"nested new game plus retry cannot leak staged A");mode=7;C(Import("A")==0&&Is("B"),"nested native loaded destination is preserved on success");mode=12;try{Import("A");C(false,"post-reset exception propagated");}catch(const std::runtime_error&){}C(g_markers.empty(),"post-reset throw cannot restore previous or staged world");
 mode=8;std::string mutableSource="A";C(Import(mutableSource)==0&&Is("A")&&mutableSource=="B","source marker data is owned before native mutates source arguments");
 mode=0;SaveFileSystem_newGame_hook(&fs);C(g_markers.empty(),"new game clears previous and failed import bookmarks");Save("NEW");Load("NEW");C(g_markers.empty(),"new game first save does not merge other worlds");C(Import("A")==0&&Is("A"),"title/new-game import does not require old active slot");
 Load("B");C(Import("EMPTY")==0&&g_markers.empty(),"explicit empty source stays empty");Load("B");C(Import("MISSING")==0&&g_markers.empty(),"missing selected source never borrows active B");
 mode=3;C(Import("A")==0&&Is("DEST"),"existing native destination sidecar is preserved");mode=4;C(Import("A")==0&&g_markers.empty(),"invalid existing destination is not overwritten with source");C(Text(fs.readFile(kMarkerPersistenceFileName))=="damaged destination","destination forensic bytes stay intact");
 mode=0;Load("A");g_markers=One("EDIT");Save("SAVEAS");C(Text(root+"/A/Map-markers.json")==originalA,"registered copy-on-write leaves source slot unchanged");Load("SAVEAS");C(Is("EDIT"),"edited bookmarks saved in Save As destination");
 mode=21;g_markers=One("UNSAVED");C(!SaveFileSystem_saveGame_hook(&fs,root+"/SAVEAS"),"rejected save result forwarded");mode=0;Load("SAVEAS");C(Is("EDIT"),"rejected save cannot overwrite prior destination sidecar");
 mode=0;std::string legacy=root+"/LEGACY";CreateDirectoryA(legacy.c_str(),NULL);{std::ofstream f((legacy+"/Map-markers.json").c_str());f<<"{\"x\":0.1,\"y\":0.2}";}C(Import("LEGACY")==0&&g_markers.size()==1&&g_markers[0].type==MarkerType_Note,"existing legacy single-marker saves remain importable");Save("LEGACY-SAVED");Load("LEGACY-SAVED");C(g_markers.size()==1&&g_markers[0].normalizedX>.09f&&g_markers[0].normalizedX<.11f,"legacy marker survives native save registration");
 std::cout<<"PASS "<<checks<<" actual bookmark import/save lifecycle checks\n";return 0;
}
'''.replace('@PURE@', pure).replace('@ACTUAL@', actual).replace('@INVOKE@', invoke)
VC = Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC')
SDK = Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env = os.environ.copy()
env['INCLUDE'] = f'{VC}/include;{SDK}/Include;{ROOT}/src'
env['LIB'] = f'{VC}/lib/amd64;{SDK}/Lib/x64'
def run(name, code, opt, negative=False):
    file = OUT/(name+'.cpp')
    file.write_text(code, encoding='utf-8-sig')
    exe = file.with_suffix('.exe')
    result = subprocess.run([str(VC/'bin/amd64/cl.exe'), '/nologo', '/EHsc', '/'+opt,
                             '/MT', str(file), str(ROOT/'src/MapMarkersPersistence.cpp'),
                             '/Fo'+str(OUT)+'\\', '/Fe'+str(exe)], env=env, cwd=OUT,
                            capture_output=True, text=True)
    print(result.stdout+result.stderr, end='')
    assert result.returncode == 0
    owned = OUT/('owned-'+uuid.uuid4().hex)
    owned.mkdir()
    result = subprocess.run([str(exe), str(owned)], capture_output=True, text=True)
    print(result.stdout+result.stderr, end='')
    assert (result.returncode != 0 and 'FAIL ' in result.stdout) if negative else result.returncode == 0


for opt in ['Od', 'O2']:
    run('lifecycle-'+opt, fixture, opt)
mutants = [
    ('wrong-source', 'JoinWindowsPath(location, name.c_str()), kMarkerPersistenceFileName',
     'GetActiveSaveDirectory(), kMarkerPersistenceFileName'),
    ('failed-import', 'if (result == 0)', 'if (true)'),
    ('unregistered', 'fs->writeFile(kMarkerPersistenceFileName), g_markers, logSuccess',
     'GetMarkerPersistencePath(), g_markers, logSuccess'),
]
for name, before, after in mutants:
    assert before in fixture, name
    run('mutant-'+name, fixture.replace(before, after, 1), 'O2', True)
print('PASS three compiled import ownership negative controls')
