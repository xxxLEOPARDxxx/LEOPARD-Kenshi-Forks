// Uses the unmodified shared production loader; only the game's locale service
// and log sink are substituted by the isolated runner's include directory.
#define KLOC_DOMAIN "organize_the_trader"
#include <Localization.h>
#include "../src/TraderLayoutRecognition.h"
#include <cstdlib>
#include <iostream>

static void Check(bool ok, const char* message)
{
    if (!ok) { std::cerr << "FAIL: " << message << std::endl; std::exit(1); }
}

struct Widget
{
    std::string name, type, caption;
    std::vector<Widget*> children;
    Widget(const char* n, const char* t) : name(n), type(t) {}
    ~Widget() { for (size_t i=0;i<children.size();++i) delete children[i]; }
    const std::string& getName() const { return name; }
    const std::string& getTypeName() const { return type; }
    size_t getChildCount() const { return children.size(); }
    Widget* getChildAt(size_t i) { return children[i]; }
};

int main(int argc, char** argv)
{
    Check(argc == 3, "locale and English-key fixture arguments");
    LocaleManager::testLocale.id = argv[1];
    if (LocaleManager::testLocale.id == "<no-manager>") LocaleManager::available = false;
    if (LocaleManager::testLocale.id == "<no-locale>") LocaleManager::localeAvailable = false;

    std::ifstream fixture(argv[2]);
    Check(fixture.good(), "open English keys fixture");
    std::string key;
    int keys=0;
    const char* stable=0;
    while (std::getline(fixture,key))
    {
        const size_t tab=key.find('\t');
        Check(tab != std::string::npos, "key and English text fixture");
        const std::string english=key.substr(tab+1);
        key.erase(tab);
        Check(std::string(Tr(key.c_str())) == english, "unsupported language keeps English");
        ++keys;
    }
    Check(keys > 0, "nonempty packaged English catalog");
    Check(KLoc::Catalog().size() == static_cast<size_t>(keys), "only English catalog loaded");
    Check(std::string(Tr("Uncatalogued English fallback")) == "Uncatalogued English fallback", "missing key fallback");
#ifdef TEST_PUBLISHED_LOADER
    Check(TestLogs::debug == 1 && TestLogs::errors == 0, "published loader summary only");
#else
    Check(TestLogs::debug == 0 && TestLogs::errors == 0, "quiet successful unsupported language fallback");
#endif
    stable = Tr("Search items...");
    const int localeReads = LocaleManager::reads;
#ifndef TEST_PUBLISHED_LOADER
    KLocLog::SetEnabled(true);
    Check(TestLogs::debug == 1, "late debug enable reports cached summary once");
#endif
    for (int i=0;i<10000;++i)
    {
        Check(Tr("Search items...") == stable, "translation pointer remains valid");
        Check(std::string(stable) == "Search items...", "English pointer contents");
    }
    Check(LocaleManager::reads == localeReads, "no per-translation locale polling");
    Check(TestLogs::debug == 1 && TestLogs::errors == 0, "no repeated success logging");

    Widget trader("Root", "Window");
    const char* names[] = {"ArrangeButton", "scrollview_backpack_content", "backpack_content", "datapanel"};
    for (size_t i=0;i<4;++i) trader.children.push_back(new Widget(names[i], "Widget"));
    const char* captions[] = {"TRADER", "HAENDLER", "H\xC3\x84NDLER", "", "A merchant's name"};
    for (size_t i=0;i<sizeof(captions)/sizeof(captions[0]);++i)
    {
        trader.caption = captions[i];
        Check(TraderLayoutRecognition::IsTrader(&trader), "trader layout independent of translated caption");
    }
    std::cout << "PASS locale=" << argv[1] << " English keys=" << keys
              << " stable translations=10000 captions=5" << std::endl;
}
