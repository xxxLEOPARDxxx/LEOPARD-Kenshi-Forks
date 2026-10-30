#pragma once

#include <string>

namespace ModPath
{
// Directory holding the running module, with a trailing separator. Resolved
// from this function's own address, so the DLL's filename doesn't matter and
// the result is independent of the process working directory. Falls back to
// the host exe's directory if the module lookup fails. Cached on first call,
// which is startPlugin on the main thread (MSVC 2010 has no thread-safe
// statics), so later calls from any thread only read.
//
// UTF-8 encoded so non-ANSI install paths survive the std::string round trip;
// pass it through ToWide before handing it to a file API.
std::string Dir();

// Same directory as Dir(), as a native wide path.
std::wstring DirW();

// UTF-8 (or, for bytes that are not valid UTF-8, the ANSI code page) to wide.
std::wstring ToWide(const std::string& s);

std::string ToUtf8(const std::wstring& s);
} // namespace ModPath
