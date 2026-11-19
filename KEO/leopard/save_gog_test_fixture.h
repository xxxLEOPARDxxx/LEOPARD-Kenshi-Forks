#pragma once
#include <windows.h>

// The Python test writes an RVA-layout image to a private temporary file.
// Map it as ordinary writable data. No game instruction may be executed.
class SaveGogTestImage {
public:
    unsigned char* bytes;
    bool requested;

    SaveGogTestImage() : bytes(NULL), requested(false) {
        wchar_t path[1024];
        DWORD length = GetEnvironmentVariableW(L"KEO_GOG_SAVE_TEST_IMAGE", path, 1024);
        if(!length)
            return;
        requested = true;
        if(length >= 1024)
            return;
        HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL, NULL);
        if(file == INVALID_HANDLE_VALUE)
            return;
        const DWORD size = 0x232a000;
        if(GetFileSize(file, NULL) == size) {
            bytes = static_cast<unsigned char*>(VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
            DWORD actual = 0;
            if(bytes && (!ReadFile(file, bytes, size, &actual, NULL) || actual != size)) {
                VirtualFree(bytes, 0, MEM_RELEASE);
                bytes = NULL;
            }
        }
        CloseHandle(file);
    }

    ~SaveGogTestImage() {
        if(bytes)
            VirtualFree(bytes, 0, MEM_RELEASE);
    }

    HMODULE Module() const { return reinterpret_cast<HMODULE>(bytes); }

private:
    SaveGogTestImage(const SaveGogTestImage&);
    SaveGogTestImage& operator=(const SaveGogTestImage&);
};
