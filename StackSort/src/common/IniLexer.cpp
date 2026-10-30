#include <cerrno>
#include <climits>
#include <cstdlib>

#include "IniLexer.h"
#include "StrUtil.h"

static std::string StripComments(const std::string& line)
{
    size_t pos = line.find('#');
    if (pos == std::string::npos) return line;
    return line.substr(0, pos);
}

static std::string Trim(const std::string& s)
{
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

void IniLex(std::istream& in, std::vector<IniEntry>& out, std::vector<std::string>& warnings)
{
    std::string line;
    int lineNo     = 0;
    bool firstLine = true;

    while (std::getline(in, line))
    {
        ++lineNo;

        if (firstLine)
        {
            firstLine = false;
            if (line.size() >= 3 && (unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB &&
                (unsigned char)line[2] == 0xBF)
                line.erase(0, 3);
        }

        std::string body = Trim(StripComments(line));
        if (body.empty()) continue;

        if (body[0] == '[')
        {
            warnings.push_back(IntToStr(lineNo) + ": section headers are not used, ignoring '" + body + "'");
            continue;
        }

        size_t eq = body.find('=');
        if (eq == std::string::npos)
        {
            warnings.push_back(IntToStr(lineNo) + ": no '=' in line, ignoring '" + body + "'");
            continue;
        }

        IniEntry e;
        e.line  = lineNo;
        e.key   = Trim(body.substr(0, eq));
        e.value = Trim(body.substr(eq + 1));
        out.push_back(e);
    }
}

bool IniParseInt(const std::string& value, int& out)
{
    const char* s = value.c_str();
    char* endp    = NULL;
    errno         = 0;
    long v        = std::strtol(s, &endp, 10);
    if (endp == s || *endp != '\0' || errno == ERANGE || v < INT_MIN || v > INT_MAX) return false;
    out = (int)v;
    return true;
}

bool IniParseBool(const std::string& value, bool& out)
{
    std::string lower = ToLowerAscii(value);
    if (lower == "true" || lower == "1" || lower == "yes" || lower == "on")
    {
        out = true;
        return true;
    }
    if (lower == "false" || lower == "0" || lower == "no" || lower == "off")
    {
        out = false;
        return true;
    }
    return false;
}
