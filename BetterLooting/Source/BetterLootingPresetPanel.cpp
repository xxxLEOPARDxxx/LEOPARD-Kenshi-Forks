#define KLOC_DOMAIN "better_looting"
#include <Localization.h>
#include <ModConfigMenu.h>
#include <GameTheme.h>
#include <WidgetRef.h>
#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_EditBox.h>
#include <mygui/MyGUI_Gui.h>
#include <kenshi/gui/OptionsWindow.h>
#include <kenshi/gui/ToolTip.h>
#include <algorithm>
#include "BetterLootingPresets.h"
#include "BetterLootingPresetPanel.h"

namespace BetterLootingPresetPanel
{
    namespace
    {
        using namespace BetterLootingPresets;

        struct Row
        {
            MyGUI::EditBox* name;
            MyGUI::TextBox* status;
            MyGUI::Button* apply;
            MyGUI::Button* save;
            MyGUI::Button* clear;
        };

        struct Request
        {
            int slot;
            Operation operation;
            std::string names[SlotCount];
            Request() : slot(-1), operation(Apply) {}
        };

        WidgetRef g_root;
        WidgetRef g_pageButton;
        Row g_rows[SlotCount];
        std::string g_ini;
        void (__cdecl *g_reload)() = NULL;
        MCM_OpenPageFn g_openPage = NULL;
        Request g_pending;
        Result g_result = Ok;
        std::string g_draftNames[SlotCount];
        bool g_restoreDrafts = false;
        bool g_requestDetached = false;

        void Tip(MyGUI::Widget* widget, const char* text)
        {
            OptionsWindow* options = OptionsWindow::getSingleton();
            if (options != NULL && options->tooltip != NULL)
                options->tooltip->setup(widget, text);
        }

        MyGUI::TextBox* Label(MyGUI::Widget* parent, const MyGUI::IntCoord& rect, MyGUI::Align align,
                              const MyGUI::Colour& colour)
        {
            MyGUI::TextBox* label = parent->createWidget<MyGUI::TextBox>(
                "Kenshi_TextboxStandardText", rect, MyGUI::Align::Default);
            label->setTextColour(colour);
            label->setTextAlign(align | MyGUI::Align::VCenter);
            label->setNeedMouseFocus(false);
            return label;
        }

        // A floppy icon drawn with the shared solid skin, tinted from the
        // current interface palette. It renders with every game font.
        void SaveIcon(MyGUI::Button* button, int size, const MyGUI::Colour& ink)
        {
            static const int strokes[][4] = {
                { 1, 1, 11, 2 }, { 1, 3, 2, 10 }, { 11, 2, 2, 11 },
                { 3, 11, 8, 2 }, { 4, 3, 5, 3 }, { 5, 8, 4, 1 }
            };
            const int origin = (size - 14) / 2;
            for (size_t i = 0; i < sizeof(strokes) / sizeof(strokes[0]); ++i)
            {
                const int* p = strokes[i];
                MyGUI::Widget* stroke = button->createWidget<MyGUI::Widget>(
                    "WhiteSkin", MyGUI::IntCoord(origin + p[0], origin + p[1], p[2], p[3]),
                    MyGUI::Align::Center);
                stroke->setColour(ink);
                stroke->setNeedMouseFocus(false);
            }
        }

        const char* ErrorText(Result result)
        {
            if (result == ValueTooLong)
                return Tr("Preset could not be saved. Shorten its name or item list.");
            if (result == EmptySlot)
                return Tr("This preset is empty or incomplete.");
            return Tr("Could not write the preset file. Check file permissions.");
        }

        void PaintStatus(const State& state)
        {
            const bool usable = g_pageButton.get() != NULL;
            for (int i = 0; i < SlotCount; ++i)
            {
                const bool occupied = state.slots[i].occupied;
                const bool active = state.active == i;
                Row& row = g_rows[i];
                if (row.clear == NULL)
                    continue;
                row.apply->setStateSelected(active);
                row.apply->setEnabled(usable && occupied);
                row.save->setEnabled(usable);
                row.clear->setEnabled(usable && occupied);
                if (active && state.modified)
                    row.status->setCaption(Tr("Modified"));
                else if (!occupied)
                    row.status->setCaption(Tr("Empty"));
                else
                    row.status->setCaption(Tr("Saved"));
            }
        }

        MyGUI::Button* SelectedModButton(MyGUI::Widget* area)
        {
            MyGUI::Widget* right = area->getParent();
            MyGUI::Widget* tab = right != NULL ? right->getParent() : NULL;
            MyGUI::Widget* list = tab != NULL ? tab->findWidget("MCM_ModList") : NULL;
            if (list == NULL)
                return NULL;
            MyGUI::Widget* client = list->getClientWidget();
            if (client != NULL)
                list = client;
            for (size_t i = 0; i < list->getChildCount(); ++i)
            {
                MyGUI::Button* button = list->getChildAt(i)->castType<MyGUI::Button>(false);
                if (button != NULL && button->getStateSelected() && !button->getUserString("mcm_index").empty())
                    return button;
            }
            return NULL;
        }

        bool PageStillSelected()
        {
            MyGUI::Button* button = static_cast<MyGUI::Button*>(g_pageButton.get());
            return button != NULL && button->getStateSelected() && button->getInheritedVisible();
        }

        void OnButton(MyGUI::Widget* sender)
        {
            if (g_root.get() == NULL || g_pending.slot != -1 || g_openPage == NULL ||
                g_rows[SlotCount - 1].clear == NULL || !PageStillSelected())
                return;
            for (int i = 0; i < SlotCount; ++i)
            {
                const Row& row = g_rows[i];
                if (sender != row.apply && sender != row.save && sender != row.clear)
                    continue;
                g_requestDetached = false;
                g_pending.slot = i;
                g_pending.operation = sender == row.save ? Save : (sender == row.clear ? Clear : Apply);
                for (int j = 0; j < SlotCount; ++j)
                    g_pending.names[j] = g_rows[j].name->getOnlyText().asUTF8();
                g_openPage("BetterLooting");
                return;
            }
        }

        MyGUI::Button* Button(MyGUI::Widget* parent, const char* skin, int x, int y, int size, const char* tip)
        {
            MyGUI::Button* button = parent->createWidget<MyGUI::Button>(
                skin, MyGUI::IntCoord(x, y, size, size), MyGUI::Align::Default);
            button->setNeedKeyFocus(false);
            button->eventMouseButtonClick += MyGUI::newDelegate(&OnButton);
            Tip(button, tip);
            return button;
        }

        void DestroyPanel()
        {
            MyGUI::Widget* root = g_root.get();
            g_root.reset();
            for (int i = 0; i < SlotCount; ++i)
                g_rows[i] = Row();
            if (root != NULL && MyGUI::Gui::getInstancePtr() != NULL)
                MyGUI::Gui::getInstance().destroyWidget(root);
        }

        void __cdecl Detach(void*)
        {
            // MCM changes the selection before it detaches the old page.
            // A queued rebuild superseded by a different tab must not run later.
            g_requestDetached = g_root.get() != NULL && g_pending.slot != -1 && PageStillSelected();
            if (!g_requestDetached)
                g_pending = Request();
            DestroyPanel();
        }

        void __cdecl BeforeSettingsRows(MCM_Api*, void*)
        {
            if (!g_requestDetached || g_pending.slot == -1 || !PageStillSelected())
            {
                g_pending = Request();
                return;
            }
            g_requestDetached = false;
            // This callback runs after MCM's forced settings flush and
            // before it reads the INI to build the replacement controls.
            const int slot = g_pending.slot;
            const Operation operation = g_pending.operation;
            g_pending.slot = -1;
            for (int i = 0; i < SlotCount; ++i)
                g_draftNames[i].swap(g_pending.names[i]);
            g_pending = Request();
            g_restoreDrafts = true;
            g_result = Execute(g_ini, slot, operation, g_draftNames[slot]);
            if (g_result == Ok)
            {
                if (operation == Apply)
                    g_reload();
                else if (operation == Clear)
                    g_draftNames[slot].clear();
            }
            else
            {
                char message[96];
                sprintf_s(message, "BetterLooting: preset action failed (code %d)", static_cast<int>(g_result));
                ErrorLog(message);
            }
        }

        void __cdecl Attach(void*, void* parent)
        {
            MyGUI::Widget* area = static_cast<MyGUI::Widget*>(parent);
            MyGUI::Widget* previous = g_root.get();
            // MCM can reattach on the same area after a pause before it
            // processes its queued refresh. Keep that particular request.
            if (g_pending.slot != -1 && previous != NULL && previous->getParent() == area)
                return;
            g_requestDetached = false;
            g_pending = Request();
            DestroyPanel();
            if (area == NULL)
                return;
            g_pageButton.set(SelectedModButton(area));
            if (g_pageButton.get() == NULL)
                ErrorLog("BetterLooting: selected MCM page button unavailable");

            const MyGUI::IntSize size = area->getSize();
            const int margin = 10, header = 22, gap = 4;
            const int step = (std::min)(33, (size.height - header - gap) / SlotCount);
            const int buttonSize = step - 2;
            if (buttonSize < 14 || size.width < 190)
            {
                ErrorLog("BetterLooting: MCM custom area is too small for preset controls");
                return;
            }
            MyGUI::Widget* root = area->createWidget<MyGUI::Widget>("PanelEmpty",
                MyGUI::IntCoord(0, 0, size.width, size.height), MyGUI::Align::Stretch);
            g_root.set(root);

            const State state = ReadState(g_ini);
            const MyGUI::Colour main = GameTheme::ReadableColour("Main", "#AFA68B");
            const int right = size.width - margin;
            const int applyX = right - 3 * (buttonSize + gap);
            const int statusW = (std::min)(108, applyX / 3);
            const int nameX = margin + 26;
            const int nameW = applyX - nameX - statusW - 2 * gap;
            Label(root, MyGUI::IntCoord(margin, 0, right - margin, header),
                MyGUI::Align::Left, main)->setCaption(g_result == Ok ? Tr("Preset slots") : ErrorText(g_result));

            for (int i = 0; i < SlotCount; ++i)
            {
                const int y = header + i * step;
                Row& row = g_rows[i];
                char number[8];
                sprintf_s(number, "%d", i + 1);
                Label(root, MyGUI::IntCoord(margin, y, 22, buttonSize),
                      MyGUI::Align::Center, main)->setCaption(number);
                row.name = root->createWidget<MyGUI::EditBox>(
                    "Kenshi_EditBox", MyGUI::IntCoord(nameX, y, nameW, buttonSize), MyGUI::Align::Default);
                row.name->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
                row.name->setMaxTextLength(40);
                row.name->setOnlyText(g_restoreDrafts ? g_draftNames[i] : state.slots[i].name);
                Tip(row.name, Tr("Optional preset name. Click Save to store it with your current rules."));
                row.status = Label(root, MyGUI::IntCoord(nameX + nameW + gap, y, statusW, buttonSize),
                    MyGUI::Align::Left, main);
                row.apply = Button(root, "Kenshi_TickBoxSkin", applyX, y, buttonSize,
                    Tr("Apply this preset to the current looting rules."));
                row.save = Button(root, "Kenshi_Button1Skin", applyX + buttonSize + gap, y, buttonSize,
                    Tr("Save your current looting rules in this slot."));
                SaveIcon(row.save, buttonSize, main);
                row.clear = Button(root, "Kenshi_CloseButtonSkin", applyX + 2 * (buttonSize + gap), y, buttonSize,
                    Tr("Clear this slot and keep your current looting rules."));
            }
            PaintStatus(state);
            g_restoreDrafts = false;
            for (int i = 0; i < SlotCount; ++i)
                g_draftNames[i].clear();
        }
    }

    void Register(MCM_Api* api, const std::string& ini, void (__cdecl *reload)())
    {
        if (api->version < 6 || api->customArea == NULL || api->rows == NULL)
            return;

        HMODULE mcm = GetModuleHandleA("ModConfigMenu.dll");
        g_openPage = mcm != NULL
            ? reinterpret_cast<MCM_OpenPageFn>(GetProcAddress(mcm, "MCM_OpenPage")) : NULL;
        if (g_openPage == NULL)
        {
            ErrorLog("BetterLooting: MCM_OpenPage unavailable for preset controls");
            return;
        }
        g_ini = ini;
        g_reload = reload;
        g_pageButton.reset();
        g_pending = Request();
        g_restoreDrafts = false;
        g_requestDetached = false;
        g_result = Ok;
        api->customArea(api, &Attach, &Detach, NULL, 38);
        api->rows(api, &BeforeSettingsRows, NULL);
    }

    void UpdateStatus()
    {
        if (g_root.get() != NULL)
            PaintStatus(ReadState(g_ini));
    }
}
