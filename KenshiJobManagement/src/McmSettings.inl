// SPDX-License-Identifier: GPL-3.0-only
// LEOPARD fork: plug-in settings edited in Mod Config Menu (MCM tab of the
// game's Options window). They live in KenshiJobManagement.ini next to the
// DLL; the mod ships KenshiJobManagement.defaults.ini, and the player's ini is
// created from it and only gains new keys on updates (shared/IniDefaults.h).
// The manager's own window state (station filters, collapsed groups) stays in
// %LOCALAPPDATA%\KenshiJobManagement\settings.ini as in the original.

#ifndef KENSHI_JOB_MANAGEMENT_MCM_SETTINGS_INL
#define KENSHI_JOB_MANAGEMENT_MCM_SETTINGS_INL

    bool g_settingHudButton = true;      // the "JM" button on the game's panel
    int g_settingUiScale = 100;          // interface size, percent of the base scale

    // One scale for the whole manager: the original was laid out for 1080
    // lines with 11-16 px fonts, unreadable on larger screens. Base = screen
    // height / 1080, times the MCM size setting. S() - a length at that scale.
    float g_uiScale = 1.0f;
    // The game itself scales template parts (scroll bar arrows and thumbs)
    // by screen height / 1080 - 43 px instead of 32 at 1440 lines. Widgets
    // built on such templates take their size from here, not from S().
    float g_gameGuiScale = 1.0f;

    int S(int value)
    {
        return static_cast<int>(value * g_uiScale + (value >= 0 ? 0.5f : -0.5f));
    }

    int NativeGui(int value)
    {
        // rounded up, as the game rounds its template parts (34 -> 46 at 1440)
        return static_cast<int>(std::ceil(value * g_gameGuiScale - 0.001f));
    }

    void ComputeUiScale()
    {
        float base = 1.0f;
        MyGUI::RenderManager* const render = MyGUI::RenderManager::getInstancePtr();
        if (render != NULL && render->getViewSize().height > 0)
        {
            base = render->getViewSize().height / 1080.0f;
        }
        g_gameGuiScale = base;
        g_uiScale = base * g_settingUiScale / 100.0f;
        if (g_uiScale < 0.75f)
        {
            g_uiScale = 0.75f;
        }
        if (g_uiScale > 3.0f)
        {
            g_uiScale = 3.0f;
        }
    }
    // The game's own fonts at their own sizes. Stretching one font with
    // setFontHeight beyond its size crumples the glyphs (user, 12.10.2026),
    // so a size request picks the largest game font not above it, and
    // fitting steps down one font at a time. Sizes come from the fonts
    // actually loaded: a translation can replace them (the Russian build's
    // painted fonts are 25/28/35, vanilla 20/25/28).
    //
    // Two ladders, like the game itself: plain text - StandardFont; button
    // captions - the game's painted button font, falling back to
    // StandardFont when a caption does not fit a small button.
    struct KjmFont
    {
        const char* name;
        int px;
    };

    KjmFont g_textLadder[] =
    {
        { "Kenshi_StandardFont_Small", 14 },
        { "Kenshi_StandardFont_MediumSmall", 16 },
        { "Kenshi_StandardFont_Medium", 20 },
        { "Kenshi_StandardFont_Large", 24 }
    };
    KjmFont g_buttonLadder[] =
    {
        { "Kenshi_StandardFont_Small", 14 },
        { "Kenshi_StandardFont_MediumSmall", 16 },
        { "Kenshi_StandardFont_Medium", 20 },
        { "Kenshi_PaintedTextFont_Small", 20 },
        { "Kenshi_PaintedTextFont_Medium", 25 },
        { "Kenshi_PaintedTextFont_Large", 28 }
    };
    const int KJM_TEXT_FONTS = sizeof(g_textLadder) / sizeof(g_textLadder[0]);
    const int KJM_BUTTON_FONTS = sizeof(g_buttonLadder) / sizeof(g_buttonLadder[0]);
    bool g_fontSizesKnown = false;

    void SortLadder(KjmFont* ladder, int count)
    {
        // insertion sort by size; equal sizes keep their order (standard
        // before painted - the painted one wins a tie in FontIndexForPx)
        for (int i = 1; i < count; ++i)
        {
            const KjmFont item = ladder[i];
            int j = i - 1;
            while (j >= 0 && ladder[j].px > item.px)
            {
                ladder[j + 1] = ladder[j];
                --j;
            }
            ladder[j + 1] = item;
        }
    }

    void LoadFontSizes(KjmFont* ladder, int count)
    {
        MyGUI::FontManager* const fonts = MyGUI::FontManager::getInstancePtr();
        for (int i = 0; fonts != NULL && i < count; ++i)
        {
            MyGUI::IFont* const font = fonts->getByName(ladder[i].name);
            if (font != NULL && font->getDefaultHeight() > 0)
            {
                ladder[i].px = font->getDefaultHeight();
            }
        }
        SortLadder(ladder, count);
    }

    void EnsureFontSizes()
    {
        if (g_fontSizesKnown || MyGUI::FontManager::getInstancePtr() == NULL)
        {
            return;
        }
        g_fontSizesKnown = true;
        LoadFontSizes(g_textLadder, KJM_TEXT_FONTS);
        LoadFontSizes(g_buttonLadder, KJM_BUTTON_FONTS);
    }

    void LadderFor(MyGUI::TextBox* text, KjmFont** ladder, int* count)
    {
        EnsureFontSizes();
        if (text != NULL && text->castType<MyGUI::Button>(false) != NULL)
        {
            *ladder = g_buttonLadder;
            *count = KJM_BUTTON_FONTS;
        }
        else
        {
            *ladder = g_textLadder;
            *count = KJM_TEXT_FONTS;
        }
    }

    int FontIndexForPx(const KjmFont* ladder, int count, int px)
    {
        int best = 0;
        for (int i = 0; i < count; ++i)
        {
            if (ladder[i].px <= px)
            {
                best = i;
            }
        }
        return best;
    }

    int FontIndexOf(MyGUI::TextBox* text, const KjmFont* ladder, int count)
    {
        const std::string& name = text->getFontName();
        for (int i = 0; i < count; ++i)
        {
            if (name == ladder[i].name)
            {
                return i;
            }
        }
        return FontIndexForPx(ladder, count, text->getFontHeight());
    }

    void SetFontIndex(MyGUI::TextBox* text, const KjmFont* ladder, int count, int index)
    {
        index = index < 0 ? 0 : (index >= count ? count - 1 : index);
        if (text->getFontName() != ladder[index].name)
        {
            text->setFontName(ladder[index].name);
        }
        text->setFontHeight(ladder[index].px);
    }

    // A size request (already scaled with S()) -> the matching game font.
    void SetFontPx(MyGUI::TextBox* text, int px)
    {
        if (text == NULL)
        {
            return;
        }
        KjmFont* ladder = NULL;
        int count = 0;
        LadderFor(text, &ladder, &count);
        SetFontIndex(text, ladder, count, FontIndexForPx(ladder, count, px));
    }

    // One game font smaller; false when already the smallest allowed
    // (minPx - the smallest size the caller accepts, scaled).
    bool StepFontDown(MyGUI::TextBox* text, int minPx = 0)
    {
        if (text == NULL)
        {
            return false;
        }
        KjmFont* ladder = NULL;
        int count = 0;
        LadderFor(text, &ladder, &count);
        const int index = FontIndexOf(text, ladder, count);
        const int minIndex = minPx > 0 ? FontIndexForPx(ladder, count, minPx) : 0;
        if (index <= 0 || index - 1 < minIndex)
        {
            return false;
        }
        SetFontIndex(text, ladder, count, index - 1);
        return true;
    }

    void StepFontUp(MyGUI::TextBox* text)
    {
        if (text == NULL)
        {
            return;
        }
        KjmFont* ladder = NULL;
        int count = 0;
        LadderFor(text, &ladder, &count);
        SetFontIndex(text, ladder, count, FontIndexOf(text, ladder, count) + 1);
    }

    HoldKey::Key g_hotkey;               // opens / closes the manager
    std::string g_hotkeyText = "CTRL+J";

    void RestoreHudManagerButton();      // HudButton.inl
    void ApplyUiScale();                 // UiScale.inl

    std::string LeopardIniPath()
    {
        char path[MAX_PATH] = { 0 };
        const DWORD length = GetModuleFileNameA(
            reinterpret_cast<HMODULE>(&__ImageBase), path, MAX_PATH);
        if (length == 0 || length >= MAX_PATH)
        {
            return std::string();
        }
        std::string file(path, length);
        const std::string::size_type dot = file.rfind('.');
        if (dot == std::string::npos)
        {
            return std::string();
        }
        return IniDefaults::Prepare(file.substr(0, dot) + ".ini");
    }

    bool LeopardEnabledSetting()
    {
        const std::string ini = LeopardIniPath();
        return ini.empty() ||
            GetPrivateProfileIntA("General", "Enabled", 1, ini.c_str()) != 0;
    }

    void LoadLeopardSettings()
    {
        const std::string ini = LeopardIniPath();
        KLocLog::SetEnabled(!ini.empty() && GetPrivateProfileIntA("General", "Debug", 0, ini.c_str()) != 0);
        if (ini.empty())
        {
            return;
        }
        char key[64] = { 0 };
        GetPrivateProfileStringA("General", "Hotkey", "CTRL+J", key, sizeof(key), ini.c_str());
        g_hotkey = HoldKey::Parse(key);
        g_hotkeyText = g_hotkey.vk != 0 ? std::string(key) : std::string();

        g_settingUiScale = GetPrivateProfileIntA("General", "UiScale", 100, ini.c_str());
        if (g_settingUiScale < 70)
        {
            g_settingUiScale = 70;
        }
        if (g_settingUiScale > 200)
        {
            g_settingUiScale = 200;
        }

        const bool hud = GetPrivateProfileIntA("General", "HudButton", 1, ini.c_str()) != 0;
        if (g_settingHudButton && !hud && g_enabled)
        {
            RestoreHudManagerButton();   // turned off in MCM - take it away now
        }
        g_settingHudButton = hud;
    }

    // Tooltip of the panel button: the current key, not a hard-coded Ctrl+J.
    std::string HudButtonTooltip()
    {
        std::string text = Localize("ui.hud.open_manager_tooltip", "Open Job Manager");
        if (!g_hotkeyText.empty())
        {
            text += " (" + g_hotkeyText + ")";
        }
        return text;
    }

#endif
