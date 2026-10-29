#pragma once
#include <set>
#include <string>

// Whole-entry matching. ASCII case is ignored for names; UTF-8 bytes and IDs
// stay exact. No lower-case string allocations are needed on candidate lookup.
struct BetterLootingNameLess
{
    static unsigned char Lower(unsigned char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; }
    bool operator()(const std::string& a, const std::string& b) const
    {
        const size_t n = a.size() < b.size() ? a.size() : b.size();
        for (size_t i = 0; i < n; ++i)
        {
            const unsigned char x = Lower((unsigned char)a[i]), y = Lower((unsigned char)b[i]);
            if (x != y) return x < y;
        }
        return a.size() < b.size();
    }
};

class BetterLootingRequiredItems
{
    std::set<std::string> ids;
    std::set<std::string, BetterLootingNameLess> names;
    static std::string Trim(const std::string& s)
    {
        const size_t a = s.find_first_not_of(" \t\r\n");
        if (a == std::string::npos) return std::string();
        return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
    }
public:
    bool Parse(const std::string& text)
    {
        ids.clear(); names.clear();
        size_t start = 0, count = 0;
        while (start <= text.size())
        {
            size_t end = text.find_first_of(";|\r\n", start);
            if (end == std::string::npos) end = text.size();
            std::string token = Trim(text.substr(start, end - start));
            if (!token.empty())
            {
                if (++count > 128) return false;
                bool idOnly = false, nameOnly = false;
                if (token.size() >= 3 && BetterLootingNameLess::Lower(token[0]) == 'i' &&
                    BetterLootingNameLess::Lower(token[1]) == 'd' && token[2] == ':')
                { idOnly = true; token = Trim(token.substr(3)); }
                else if (token.size() >= 5 && BetterLootingNameLess::Lower(token[0]) == 'n' &&
                    BetterLootingNameLess::Lower(token[1]) == 'a' && BetterLootingNameLess::Lower(token[2]) == 'm' &&
                    BetterLootingNameLess::Lower(token[3]) == 'e' && token[4] == ':')
                { nameOnly = true; token = Trim(token.substr(5)); }
                if (!token.empty())
                {
                    if (!nameOnly) ids.insert(token);
                    if (!idOnly) names.insert(token);
                }
            }
            if (end == text.size()) break;
            start = end + 1;
        }
        return true;
    }
    bool Empty() const { return ids.empty() && names.empty(); }
    bool HasNames() const { return !names.empty(); }
    bool MatchesId(const std::string& id) const { return !id.empty() && ids.find(id) != ids.end(); }
    bool MatchesName(const std::string& name) const { return !name.empty() && names.find(name) != names.end(); }
};
