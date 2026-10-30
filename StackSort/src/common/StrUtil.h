#pragma once

#include <string>
#include <sstream>

inline std::string IntToStr(int v)
{
    std::stringstream ss;
    ss << v;
    return ss.str();
}

inline std::string ToLowerAscii(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i)
    {
        char c = s[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c + ('a' - 'A'));
        out += c;
    }
    return out;
}

// Substitute the first occurrence of token in tmpl. A translator who drops the
// token would otherwise hide the number entirely, so an absent token appends
// " " + value rather than silently discarding it.
inline std::string FormatToken(const std::string& tmpl, const std::string& token, const std::string& value)
{
    size_t pos = tmpl.find(token);
    if (pos == std::string::npos) return tmpl + " " + value;
    std::string out = tmpl;
    out.replace(pos, token.size(), value);
    return out;
}
