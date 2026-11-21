from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = r'''
#include <Windows.h>
#include <cstdio>
#include <fstream>
#include <iterator>
#include "BetterLootingPresets.h"

using namespace BetterLootingPresets;
#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

std::string Read(const std::string& path, const char* section, const char* key) {
    char text[17000];
    GetPrivateProfileStringA(section,key,"<missing>",text,sizeof(text),path.c_str());
    return text;
}

std::string Bytes(const std::string& path) {
    std::ifstream file(path.c_str(),std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

namespace PanelFixture {
namespace MyGUI {
    struct Button {
        bool selected, visible;
        bool getStateSelected() const { return selected; }
        bool getInheritedVisible() const { return visible; }
    };
}
struct MCM_Api {};
struct WidgetRef {
    MyGUI::Button* button;
    WidgetRef() : button(NULL) {}
    MyGUI::Button* get() const { return button; }
};
struct Request {
    int slot;
    Operation operation;
    std::string names[SlotCount];
    Request() : slot(-1), operation(Apply) {}
};
WidgetRef g_root,g_pageButton;
Request g_pending;
bool g_requestDetached=false,g_restoreDrafts=false;
std::string g_ini,g_draftNames[SlotCount];
Result g_result=Ok;
void Reload() {}
void (*g_reload)() = &Reload;
void ErrorLog(const char*) {}
void DestroyPanel() { g_root.button=NULL; }
// @PRODUCTION_HANDOFF@
int Test(const std::string& path) {
    MyGUI::Button mod = {true,true};
    g_ini = path;
    g_root.button = g_pageButton.button = &mod;
    g_pending.slot=0;
    g_pending.operation=Clear;
    mod.selected=false;
    Detach(NULL);
    mod.selected=true;
    BeforeSettingsRows(NULL,NULL);
    CHECK(ReadState(path).slots[0].occupied);

    g_root.button=&mod;
    g_pending.slot=0;
    g_pending.operation=Clear;
    Detach(NULL);
    BeforeSettingsRows(NULL,NULL);
    CHECK(!ReadState(path).slots[0].occupied);
    return 0;
}
}

int main(int argc, char** argv) {
    CHECK(argc == 2);
    const std::string path = argv[1];
    const char* ini = path.c_str();
    State empty = ReadState(path);
    CHECK(empty.active == -1 && !empty.modified && Bytes(path).empty());
    for (int i=0; i<SlotCount; ++i) CHECK(!empty.slots[i].occupied);
    CHECK(Execute(path,0,Apply,"") == EmptySlot && Bytes(path).empty());
    CHECK(Execute(path,SlotCount,Save,"") == InvalidSlot);

    CHECK(WritePrivateProfileStringA("Other","Keep","untouched",ini));
    CHECK(WritePrivateProfileStringA("Behavior","ActivationKey","F8",ini));
    CHECK(WritePrivateProfileStringA("Behavior","IgnorePlayerStorage","0",ini));
    CHECK(WritePrivateProfileStringA("Debug","Log","1",ini));
    CHECK(WritePrivateProfileStringA("General","LootRadius","73.5",ini));
    CHECK(WritePrivateProfileStringA("Rules","MinValue","899",ini));
    CHECK(WritePrivateProfileStringA("Rules","MinValuePerKg","67.125",ini));
    CHECK(WritePrivateProfileStringA("Rules","AllowStolen","1",ini));
    CHECK(WritePrivateProfileStringA("Categories","Weapons","0",ini));
    CHECK(WritePrivateProfileStringA("Behavior","PickupGroundItems","1",ini));
    CHECK(WritePrivateProfileStringA("Behavior","DepositByRules","1",ini));
    const std::string whitelist = "name:\xE5\x85\xBD\xE7\x9A\xAE; id:record | " + std::string(8165,'x');
    CHECK(whitelist.size() <= 8191);
    CHECK(WritePrivateProfileStringA("Rules","RequiredItems",whitelist.c_str(),ini));
    const std::string name = "\xD0\xA2\xD0\xB5\xD1\x81\xD1\x82 #2";
    for (int i = 0; i < SlotCount; ++i)
        CHECK(Execute(path,i,Save,i == 1 ? name : "") == Ok);
    State saved = ReadState(path);
    CHECK(saved.active == -1 && saved.slots[1].occupied && saved.slots[1].name == name);
    CHECK(Read(path,"Rules","MinValuePerKg") == "67.125");
    CHECK(Read(path,"General","LootRadius") == "73.5");
    CHECK(Read(path,"Rules","RequiredItems") == whitelist);
    CHECK(WritePrivateProfileStringA("Rules","RequiredItems","id:changed",ini));
    CHECK(WritePrivateProfileStringA("General","LootRadius","13",ini));
    CHECK(WritePrivateProfileStringA("Categories","Weapons","1",ini));
    CHECK(WritePrivateProfileStringA("Behavior","ActivationKey","F9",ini));
    CHECK(Execute(path,1,Apply,"") == Ok);
    State applied = ReadState(path);
    CHECK(applied.active == 1 && !applied.modified);
    CHECK(Read(path,"General","LootRadius") == "73.5");
    CHECK(Read(path,"Rules","MinValue") == "899");
    CHECK(Read(path,"Rules","MinValuePerKg") == "67.1");
    CHECK(GetPrivateProfileIntA("Rules","MinValue",0,ini) == 899);
    CHECK(WritePrivateProfileStringA("Rules","MinValuePerKg","67.10",ini));
    CHECK(!ReadState(path).modified);
    CHECK(Read(path,"Rules","AllowStolen") == "1");
    CHECK(Read(path,"Rules","RequiredItems") == whitelist);
    CHECK(Read(path,"Behavior","ActivationKey") == "F9");
    CHECK(Read(path,"Behavior","IgnorePlayerStorage") == "0");
    CHECK(Read(path,"Behavior","PickupGroundItems") == "1");
    CHECK(Read(path,"Behavior","DepositByRules") == "1");
    CHECK(Read(path,"Categories","Weapons") == "0");
    CHECK(Read(path,"Categories","Narcotics") == "0");
    CHECK(Read(path,"Other","Keep") == "untouched" && Read(path,"Debug","Log") == "1");
    CHECK(WritePrivateProfileStringA("General","LootRadius","28",ini));
    CHECK(ReadState(path).modified);
    CHECK(Execute(path,3,Clear,"") == Ok && ReadState(path).active == 1);
    CHECK(Execute(path,1,Clear,"") == Ok);
    State cleared = ReadState(path);
    CHECK(cleared.active == -1 && !cleared.slots[1].occupied && cleared.slots[0].occupied);
    CHECK(Read(path,"General","LootRadius") == "28");

    CHECK(WritePrivateProfileStringA("Rules","RequiredItems",std::string(8192,'x').c_str(),ini));
    const std::string before = Bytes(path);
    CHECK(Execute(path,0,Save,"") == ValueTooLong);
    CHECK(Bytes(path) == before);
    CHECK(Execute(path,0,Apply,"") == Ok);
    CHECK(Read(path,"Rules","RequiredItems") == whitelist);
    const std::string beforeLocked = Bytes(path);
    HANDLE locked = CreateFileA(ini,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,0);
    CHECK(locked != INVALID_HANDLE_VALUE);
    CHECK(Execute(path,0,Clear,"") == IoError);
    CloseHandle(locked);
    CHECK(Bytes(path) == beforeLocked && ReadState(path).active == 0);
    CHECK(WritePrivateProfileStringA("BetterLootingPreset1","LootRadius",NULL,ini));
    CHECK(Execute(path,0,Apply,"") == EmptySlot);
    CHECK(Read(path,"General","LootRadius") == "73.5");
    CHECK(PanelFixture::Test(path) == 0);
    puts("PASS: five persistent presets, UTF-8 / long filters, apply/clear/dirty state, global keys, truncated input, locked file and incomplete profile");
}
'''

panel = (ROOT/'Source/BetterLootingPresetPanel.cpp').read_text(encoding='ascii')
def extract(signature, next_signature):
    start = panel.index(signature)
    return panel[start:panel.index(next_signature, start)].strip()
code = '\n'.join([
    extract('bool PageStillSelected()', 'void OnButton('),
    extract('void __cdecl Detach(', 'void __cdecl Attach('),
])
FIXTURE = FIXTURE.replace('// @PRODUCTION_HANDOFF@',code)

vc = Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC')
sdk = Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env = os.environ.copy()
env.update(INCLUDE=f'{vc/"include"};{sdk/"Include"}',
           LIB=f'{vc/"lib/amd64"};{sdk/"Lib/x64"}')
with tempfile.TemporaryDirectory(prefix='better-looting-presets-') as directory:
    folder = Path(directory)
    (folder / 'fixture.cpp').write_text(FIXTURE, encoding='ascii')
    subprocess.run([str(vc/'bin/amd64/cl.exe'), '/nologo', '/EHsc', '/MD', '/O2',
                    '/I'+str(ROOT/'Source'), 'fixture.cpp',
                    str(ROOT/'Source/BetterLootingPresets.cpp'), '/Fepresets.exe'],
                   cwd=folder, env=env, check=True)
    subprocess.run([str(folder/'presets.exe'), str(folder/'personal.ini')], cwd=folder, check=True)
