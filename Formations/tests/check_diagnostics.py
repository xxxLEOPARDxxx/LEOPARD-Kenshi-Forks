"""Exercise the real MCM toggle, settings file and diagnostics sink offline."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
VCVARS = Path(r'C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat')
SHARED = ROOT.parents[2] / 'shared'
FIXTURE = r'''
#include "Formations.cpp"
#include <fstream>
#include <iterator>
#include <string>

static MCM_GetIntFn getLog = nullptr;
static MCM_SetIntFn setLog = nullptr;
static void* logContext = nullptr;
static unsigned logFlags = ~0u;
static std::string lastRow, lastDefault, lastTip;
static int failures = 0;

static void Check(bool valid, const char* label)
{
    if (!valid) { printf("FAIL: %s\n", label); ++failures; }
}

static std::string Read(const char* path)
{
    std::ifstream file(path);
    return std::string(std::istreambuf_iterator<char>(file), {});
}

static void Write(const char* path, const char* text)
{
    std::ofstream file(path);
    file << text;
}

static MCM_Api MakeApi()
{
    MCM_Api api = {};
    api.version = 6;
    api.beginMod = [](MCM_Api*, const char*, const char*, const char*, MCM_ApplyFn) {};
    api.info = [](MCM_Api*, const char*) {};
    api.section = [](MCM_Api*, const char*) {};
    api.hotkeyFn = [](MCM_Api*, const char* label, const char*, MCM_GetKeyFn, MCM_SetKeyFn, void*, unsigned) { lastRow = label; };
    api.numberFn = [](MCM_Api*, const char* label, const char*, MCM_GetFloatFn, MCM_SetFloatFn, void*, float, float, int, unsigned) { lastRow = label; };
    api.choiceFn = [](MCM_Api*, const char* label, const char*, MCM_GetIntFn, MCM_SetIntFn, void*, const int*, const char* const*, int, unsigned) { lastRow = label; };
    api.integerFn = [](MCM_Api*, const char* label, const char*, MCM_GetIntFn, MCM_SetIntFn, void*, int, int, unsigned) { lastRow = label; };
    api.textFn = [](MCM_Api*, const char* label, const char*, MCM_GetTextFn, MCM_SetTextFn, void*, unsigned) { lastRow = label; };
    api.rows = [](MCM_Api*, MCM_RowsFn, void*) { lastRow = "squad rows"; };
    api.toggleFn = [](MCM_Api*, const char* label, const char* tip, MCM_GetIntFn get, MCM_SetIntFn set, void* ud, unsigned flags) {
        lastRow = label; lastTip = tip; getLog = get; setLog = set; logContext = ud; logFlags = flags;
    };
    api.defaultValue = [](MCM_Api*, const char* val) { lastDefault = val; };
    return api;
}

int main()
{
    const char* log = "Formations_log.txt";
    const char* settings = "Formations_settings.txt";
    Write(log, "previous session\n");
    Write(settings, "Modifier=CTRL+B\nSpacing=17.5\nRetreat=400\nHold=0\nKeyLine=Q\n");
    Write("settings.cfg", "language=en_GB\n");
    g_self = GetModuleHandleW(nullptr);
    LogInit();
    LoadSettings();
    Log("quiet boot gameplay trace");
    Check(Read(log) == "previous session\n", "default-off does zero file writes and preserves existing log");
    Check(Read(settings).find("Debug=0\n") != std::string::npos, "old settings load with Debug off");

    auto api = MakeApi();
    GD_idataIndex = reinterpret_cast<IdataIndex_t>(1);
    MCM_Describe(&api);
    const bool registered = lastRow == "Detailed log" && getLog && setLog;
    Check(registered && lastDefault == "0" && logFlags == 0, "last English MCM option is live and defaults off");
    if (registered)
    {
        int value = -1;
        char error[128] = {};
        Check(getLog(logContext, &value) == 0 && value == 0, "legacy settings report off to MCM");
        Check(setLog(logContext, 1, error, sizeof(error)) == 0, "enable through actual MCM callback");
        Log("diagnostics after enabling");
        Check(Read(log).find("diagnostics after enabling\n") != std::string::npos, "enable writes immediately");
        Check(Read(settings).find("Debug=1\n") != std::string::npos, "enable persists");
        Check(setLog(logContext, 0, error, sizeof(error)) == 0, "disable through actual MCM callback");
        const std::string enabledLog = Read(log);
        Log("quiet after disabling");
        Check(Read(log) == enabledLog, "disable stops writes immediately");
        Check(Read(settings).find("Debug=0\n") != std::string::npos, "disable persists");
    }
    const std::string saved = Read(settings);
    for (const char* entry : {"Modifier=CTRL+B\n", "Spacing=17.5\n", "Retreat=400\n", "Hold=0\n", "KeyLine=Q\n"})
        Check(saved.find(entry) != std::string::npos, entry);
    Write(settings, "Debug=1\n");
    LoadSettings();
    Log("diagnostics after settings reload");
    Check(Read(log).find("diagnostics after settings reload\n") != std::string::npos, "persisted on enables diagnostics after load");
    Write(settings, "Debug=0\n");
    LoadSettings();
    formation_hud::Init(nullptr, nullptr);
    Check(Read(log).find("readout: HUD popup unavailable") != std::string::npos, "HUD admission error logs with Debug off");
    Write("settings.cfg", "language=ru_RU\n");
    MCM_Describe(&api);
    Check(lastRow == "Подробный журнал" && lastTip.find("Formations_log.txt") != std::string::npos, "last row and tooltip in Russian");
    Write("settings.cfg", "language=zh_CN\n");
    MCM_Describe(&api);
    Check(lastRow == "详细日志" && lastTip.find("Formations_log.txt") != std::string::npos, "last row and tooltip in Chinese");
    printf("Formations diagnostics checks, %d failed\n", failures);
    return failures != 0;
}
'''

with tempfile.TemporaryDirectory(prefix='formations-diagnostics-') as tmp:
    out = Path(tmp)
    (out / 'diagnostics.cpp').write_text(FIXTURE, encoding='utf-8')
    bat = out / 'build.cmd'
    bat.write_text(
        f'@echo off\ncall "{VCVARS}" >nul\n'
        f'cl /nologo /std:c++17 /EHa /utf-8 /MT /I"{ROOT}" /I"{SHARED}" diagnostics.cpp kernel32.lib user32.lib\n',
        encoding='utf-8',
    )
    env = os.environ.copy()
    env.pop('NoDefaultCurrentDirectoryInExePath', None)
    subprocess.run(['cmd.exe', '/c', str(bat)], cwd=out, env=env, check=True)
    subprocess.run([str(out / 'diagnostics.exe')], cwd=out, check=True)
