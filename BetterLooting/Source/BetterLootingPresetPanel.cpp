#define KLOC_DOMAIN "better_looting"
#include <Localization.h>
#include <ModConfigMenu.h>
#include <GameTheme.h>
#include <WidgetRef.h>
#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_EditBox.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_TabControl.h>
#include <mygui/MyGUI_TabItem.h>
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

        int PresetAreaPercent(int height)
        {
            const int minimumArea = 132;
            if (height <= 0)
                return 33;
            const int percentage = (minimumArea * 100 + height - 1) / height;
            return (std::min)(90, (std::max)(25, percentage));
        }

        struct PanelLayout
        {
            int titleX, titleY, titleHeight;
            int labelX, statusX, statusWidth, editX, editWidth;
            int rowTop, rowStep, controlHeight, buttonSize;
            int gap, applyX, saveX, clearX;
            bool fits;

            PanelLayout(int width, int height)
            {
                // Use the same label/value columns and right gutter as the
                // native options pane. The reserved height is shared by all
                // five slots instead of accumulating empty space below them.
                labelX = (std::max)(14, (std::min)(26, width / 48));
                titleX = labelX + 9;
                titleY = (std::max)(7, (std::min)(16, height / 14));
                titleHeight = height >= 154 ? 27 : 20;
                rowTop = titleY + titleHeight;
                rowStep = (height - rowTop - 8) / SlotCount;
                controlHeight = rowStep - (rowStep >= 25 ? 4 : 2);
                buttonSize = (std::min)(33, controlHeight);
                gap = 7;
                const int right = width - (std::max)(27, (std::min)(58, width / 20));
                clearX = right - buttonSize;
                saveX = clearX - buttonSize - gap;
                applyX = saveX - buttonSize - gap;

                statusX = labelX + (std::max)(67, (std::min)(116, width / 12));
                editX = (std::max)(width / 3, statusX + 56 + gap);
                statusWidth = editX - statusX - gap;
                editWidth = applyX - gap - editX;
                fits = controlHeight >= 14 && editWidth >= 75;
            }
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
        MyGUI::IntSize g_layoutSize;

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
                row.apply->setEnabled(usable && state.slots[i].valid);
                row.save->setEnabled(usable);
                row.clear->setEnabled(usable && occupied);
                if (active && state.modified)
                    row.status->setCaption(Tr("Modified"));
                else if (!occupied)
                    row.status->setCaption(Tr("Empty"));
                else if (!state.slots[i].valid)
                    row.status->setCaption(Tr("Incomplete"));
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
            // Kenshi rebinds every text layer when the options close.
            // A captionless button skin can still have a text layer.
            if (button->getSubWidgetText() != NULL)
                button->setFontName("Kenshi_PaintedTextFont_Medium");
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
            MyGUI::Widget* root = g_root.get();
            const bool applyRequest = root != NULL && root->getInheritedVisible() &&
                g_pending.slot != -1 && PageStillSelected();
            DestroyPanel();
            if (!applyRequest)
            {
                g_pending = Request();
                return;
            }

            // A same-page refresh reaches this detach after MCM flushes its
            // fields. Execute once here: the rebuild can defer or be replaced.
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
            // MCM also reattaches an unchanged area after a long frame.
            // Keep the focused name edit and any unsaved text in that case.
            if (area != NULL && previous != NULL && previous->getParent() == area &&
                area->getSize() == g_layoutSize)
            {
                PaintStatus(ReadState(g_ini));
                return;
            }
            g_pending = Request();
            DestroyPanel();
            if (area == NULL)
                return;
            g_pageButton.set(SelectedModButton(area));
            if (g_pageButton.get() == NULL)
                ErrorLog("BetterLooting: selected MCM page button unavailable");

            const MyGUI::IntSize size = area->getSize();
            const PanelLayout layout(size.width, size.height);
            if (!layout.fits)
            {
                ErrorLog("BetterLooting: MCM custom area is too small for preset controls");
                return;
            }
            MyGUI::Widget* root = area->createWidget<MyGUI::Widget>("PanelEmpty",
                MyGUI::IntCoord(0, 0, size.width, size.height), MyGUI::Align::Stretch);
            g_root.set(root);
            g_layoutSize = size;

            const State state = ReadState(g_ini);
            const MyGUI::Colour main = GameTheme::ReadableColour("Main", "#AFA68B");
            const MyGUI::Colour section(0.93f, 0.86f, 0.56f);
            Label(root, MyGUI::IntCoord(layout.titleX, layout.titleY, layout.editX - layout.titleX, layout.titleHeight),
                MyGUI::Align::Left, section)->setCaption("[ " + std::string(Tr("Preset slots")) + " ]");
            if (g_result != Ok)
            {
                Label(root, MyGUI::IntCoord(layout.editX, layout.titleY, size.width - layout.editX - 27, layout.titleHeight),
                    MyGUI::Align::Left, section)->setCaption(ErrorText(g_result));
            }
            const int fontHeight = (std::min)(17, layout.controlHeight - 2);

            for (int i = 0; i < SlotCount; ++i)
            {
                const int y = layout.rowTop + i * layout.rowStep;
                const int buttonY = y + (layout.controlHeight - layout.buttonSize) / 2;
                Row& row = g_rows[i];
                char label[64];
                sprintf_s(label, Tr("Slot %d"), i + 1);
                MyGUI::TextBox* labelBox = Label(root,
                    MyGUI::IntCoord(layout.labelX, y, layout.statusX - layout.labelX - layout.gap, layout.controlHeight),
                    MyGUI::Align::Left, main);
                labelBox->setFontHeight(fontHeight);
                labelBox->setCaption(label);
                row.name = root->createWidget<MyGUI::EditBox>("Kenshi_EditBox",
                    MyGUI::IntCoord(layout.editX, y, layout.editWidth, layout.controlHeight), MyGUI::Align::Default);
                row.name->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
                row.name->setFontHeight(fontHeight);
                row.name->setMaxTextLength(40);
                row.name->setOnlyText(g_restoreDrafts ? g_draftNames[i] : state.slots[i].name);
                Tip(row.name, Tr("Optional preset name. Click Save to store it with your current rules."));
                row.status = Label(root, MyGUI::IntCoord(layout.statusX, y, layout.statusWidth, layout.controlHeight),
                    MyGUI::Align::Left, main);
                row.status->setFontHeight(fontHeight);
                row.apply = Button(root, "Kenshi_TickBoxSkin", layout.applyX, buttonY, layout.buttonSize,
                    Tr("Apply this preset to the current looting rules."));
                row.save = Button(root, "Kenshi_Button1Skin", layout.saveX, buttonY, layout.buttonSize,
                    Tr("Save your current looting rules in this slot."));
                SaveIcon(row.save, layout.buttonSize, main);
                row.clear = Button(root, "Kenshi_CloseButtonSkin", layout.clearX, buttonY, layout.buttonSize,
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
        if (api->version < 6 || api->customArea == NULL)
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
        g_result = Ok;
        OptionsWindow* options = OptionsWindow::getSingleton();
        MyGUI::TabItem* tab = options != NULL && options->tabs != NULL && options->tabs->getItemCount() != 0
            ? options->tabs->getItemAt(0) : NULL;
        api->customArea(api, &Attach, &Detach, NULL, PresetAreaPercent(tab != NULL ? tab->getHeight() : 0));
    }

    void UpdateStatus()
    {
        if (g_root.get() != NULL)
            PaintStatus(ReadState(g_ini));
    }
}
