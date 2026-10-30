#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <vector>

#include "ModPath.h"

static std::wstring ModuleFilePath(HMODULE hm)
{
    // GetModuleFileNameW truncates silently (returning the buffer size), so
    // grow until the result fits.
    std::vector<wchar_t> buf(MAX_PATH);
    for (int attempt = 0; attempt < 8; ++attempt)
    {
        DWORD n = GetModuleFileNameW(hm, &buf[0], (DWORD)buf.size());
        if (n == 0) return std::wstring();
        if (n < buf.size()) return std::wstring(&buf[0], n);
        buf.resize(buf.size() * 2);
    }
    return std::wstring();
}

static std::wstring ComputeDirW()
{
    HMODULE hm = NULL;
    std::wstring path;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)&ComputeDirW, &hm) &&
        hm)
        path = ModuleFilePath(hm);

    if (path.empty()) path = ModuleFilePath(NULL);

    size_t pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return std::wstring();
    return path.substr(0, pos + 1);
}

std::wstring ModPath::DirW()
{
    static std::wstring s_dir;
    static bool s_ready = false;
    if (!s_ready)
    {
        s_dir   = ComputeDirW();
        s_ready = true;
    }
    return s_dir;
}

std::string ModPath::Dir()
{
    return ToUtf8(DirW());
}

std::wstring ModPath::ToWide(const std::string& s)
{
    if (s.empty()) return std::wstring();
    int n       = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.c_str(), (int)s.size(), NULL, 0);
    UINT cp     = CP_UTF8;
    DWORD flags = MB_ERR_INVALID_CHARS;
    if (n <= 0)
    {
        cp    = CP_ACP;
        flags = 0;
        n     = MultiByteToWideChar(cp, flags, s.c_str(), (int)s.size(), NULL, 0);
        if (n <= 0) return std::wstring();
    }
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(cp, flags, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

std::string ModPath::ToUtf8(const std::wstring& s)
{
    if (s.empty()) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0, NULL, NULL);
    if (n <= 0) return std::string();
    std::string out((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], n, NULL, NULL);
    return out;
}
