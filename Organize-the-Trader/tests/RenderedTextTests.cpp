#include "../src/TraderRenderedText.h"
#include <cstdio>
#include <cstdlib>
#include <string>
struct Renderer
{
    int creates, sets, measures, begins;
    Renderer() : creates(0), sets(0), measures(0), begins(0) {}
    bool Begin(const std::string&, int) { ++begins; if (creates == 0) ++creates; return true; }
    int CaptionWidth(const std::string& text) { ++sets; ++measures; return static_cast<int>(text.size()) * 10; }
};
struct Text
{
    const char* font;
    int height;
    float alpha;
    Text(const char* f, int h, float a) : font(f), height(h), alpha(a) {}
};
struct Widget
{
    Text* main;
    Widget* client;
    Widget(Text* t=0, Widget* c=0) : main(t), client(c) {}
    Text* getSubWidgetText() { return main; }
    Widget* getClientWidget() { return client; }
    // Reproduces the nonvirtual TextBox getters on the real Native EditBox.
    int getFontHeight() { return main == 0 ? 0 : main->height; }
    float getTextAlpha() { return main == 0 ? 0.0f : main->alpha; }
};
void Check(bool value, const char* reason)
{
    if (!value) { std::printf("FAIL: %s\n", reason); std::exit(1); }
}
int main()
{
    Text medium("Kenshi_StandardFont_Medium", 21, 1.0f);
    Widget client(&medium), edit(0, &client);
    Check(edit.getFontHeight() == 0 && edit.getTextAlpha() == 0.0f, "captured base-getter failure is represented");
    Widget* actual = TraderRenderedText::Resolve(&edit);
    Check(actual == &client, "EditBox must use its rendered Client, never empty root getters");
    Check(actual->getFontHeight() == 21 && actual->getTextAlpha() == 1.0f, "cash style stays visible and nonzero");
    Text headerStyles[2] = { *actual->getSubWidgetText(), *actual->getSubWidgetText() };
    Check(headerStyles[0].height == 21 && headerStyles[0].alpha == 1.0f
        && headerStyles[1].height == 21 && headerStyles[1].alpha == 1.0f,
        "merchant and deal both copy the accepted rendered style");
    Text decoy("RootDecorativeFont", 14, 0.0f);
    Widget composed(&decoy, &client);
    Check(TraderRenderedText::Resolve(&composed) == &client, "Client overrides decorative root text as Native EditBox does");
    Check(TraderRenderedText::Resolve(&client) == &client, "plain TextBox stays supported");
    Widget missing;
    Check(TraderRenderedText::Resolve(&missing) == 0, "missing text must preserve native header");
    missing.client = &missing;
    Check(TraderRenderedText::Resolve(&missing) == 0, "client cycle is bounded");
    Check(TraderRenderedText::OuterWidth(164,171,137)==202, "closed width includes actual client chrome and measured Native width");
    Check(TraderRenderedText::OuterWidth(164,171,121)==218, "popup chrome is independent of closed combo chrome");
    TraderRenderedText::MeasureCache<std::string> metrics;
    Renderer renderer;
    std::vector<std::string> captions(7, "caption");
    Check(metrics.Get("client-font",21,captions,renderer)==70, "real renderer callback supplies measured width");
    Check(renderer.creates==1 && renderer.sets==7 && renderer.measures==7, "first actual font is measured once");
    for (int frame=0;frame<1000;++frame) metrics.Get("client-font",21,captions,renderer);
    Check(renderer.begins==1 && renderer.sets==7 && renderer.measures==7 && renderer.creates==1,
        "unchanged frames and layout-only resize cause zero extra creates/captions/measurements");
    Check(metrics.refreshes==1 && metrics.captionSets==7 && metrics.widthReads==7,
        "production measurement counters remain stable on unchanged frames");
    metrics.Get("client-font",22,captions,renderer);
    Check(renderer.begins==2 && renderer.sets==14 && renderer.measures==14 && renderer.creates==1,
        "actual font-height change performs one bounded refresh");
    captions[3]="translated-caption";
    Check(metrics.Get("client-font",22,captions,renderer)==180 && renderer.measures==21,
        "locale/content change invalidates the seven-caption result once");
    std::puts("PASS: 16 rendered-text/native-client and measurement-invalidation checks");
}
