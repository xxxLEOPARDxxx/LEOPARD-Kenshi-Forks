#define KLOC_DOMAIN "better_looting"
#include <Localization.h>

#include "BetterLootingPanel.h"

// BetterLootingRules.h пользуется перечислениями предметов из игры,
// но сам их не подключает - в моде это работало лишь потому, что
// файл был один и Item.h шёл выше по тексту.
#include <kenshi/Item.h>
#include "BetterLootingRules.h"

#include <Windows.h>

#include <Debug.h>
#include <core/Functions.h>

#include <boost/scoped_ptr.hpp>

#include <kenshi/Globals.h>
#include <kenshi/gui/ForgottenGUI.h>
#include <kenshi/gui/DatapanelGUI.h>
#include <kenshi/gui/DataPanelLine.h>
#include <kenshi/gui/MainBarGUI.h>
#include <kenshi/gui/ToolTip.h>

#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_ComboBox.h>
#include <mygui/MyGUI_InputManager.h>
#include <mygui/MyGUI_LayerManager.h>
#include <mygui/MyGUI_RenderManager.h>
#include <mygui/MyGUI_Widget.h>
#include <mygui/MyGUI_Window.h>


namespace
{
    // Список клавиш сбора. Порядок тот же, что был в прежнем окне, чтобы
    // у игрока не поехал выбор. Home нарочно не первая: её занимает меню
    // управления Гильдией из Guild Escort Contracts.
    struct HotkeyEntry
    {
        int virtualKey;
        const char* name;
    };

    const HotkeyEntry HOTKEYS[] =
    {
        { VK_INSERT, "Insert" },
        { VK_HOME,   "Home"   },
        { VK_END,    "End"    },
        { VK_DELETE, "Delete" },
        { VK_PRIOR,  "PageUp" },
        { VK_NEXT,   "PageDown" },
        { VK_F1,  "F1"  }, { VK_F2,  "F2"  }, { VK_F3,  "F3"  },
        { VK_F4,  "F4"  }, { VK_F5,  "F5"  }, { VK_F6,  "F6"  },
        { VK_F7,  "F7"  }, { VK_F8,  "F8"  }, { VK_F9,  "F9"  },
        { VK_F10, "F10" }, { VK_F11, "F11" }, { VK_F12, "F12" }
    };

    const int HOTKEY_COUNT =
        static_cast<int>(sizeof(HOTKEYS) / sizeof(HOTKEYS[0]));


    // Слайдеры отдают float. Минимальная цена в правилах - int, поэтому у
    // неё отдельный посредник: панель правит его, сохранение округляет.
    float g_minValueProxy = 0.0f;


    // Клавиша сбора: выбор в списке ловим сами.
    //
    // Сначала список был привязан к activationVirtualKey по указателю, со
    // значениями - кодами клавиш (45 = Insert). Игрок не смог сменить
    // клавишу: в ini после закрытия панели осталось 45. Что именно игра
    // пишет по указателю при выборе - номер пункта или его значение, -
    // снаружи не видно (у SquadAutonomy значения совпадают с номерами,
    // и там это незаметно). Поэтому игре отдаём посредника, значения
    // пунктов - их номера, а клавишу по номеру берём из своей таблицы в
    // обработчике выбора и сразу сохраняем ini.
    int g_hotkeyIndexProxy = 0;


    int HotkeyIndexOf(int virtualKey)
    {
        for (int i = 0; i < HOTKEY_COUNT; ++i)
        {
            if (HOTKEYS[i].virtualKey == virtualKey)
                return i;
        }
        return 0;
    }


    void OnHotkeyPicked(MyGUI::ComboBox* sender, size_t index)
    {
        if (index >= static_cast<size_t>(HOTKEY_COUNT))
            return;   // ITEM_NONE - ничего не выбрано

        g_hotkeyIndexProxy = static_cast<int>(index);
        g_betterLootingRules.activationVirtualKey = HOTKEYS[index].virtualKey;
        BetterLootingSaveRules();

        char note[96];
        sprintf_s(note, "BetterLooting: looting key set to %s (%d)",
                  HOTKEYS[index].name, HOTKEYS[index].virtualKey);
        DebugLog(note);
    }


    void SetAllCategories(bool on)
    {
        g_betterLootingRules.lootWeapons = on;
        g_betterLootingRules.lootArmour = on;
        g_betterLootingRules.lootMaterials = on;
        g_betterLootingRules.lootFood = on;
        g_betterLootingRules.lootMedicine = on;
        g_betterLootingRules.lootTools = on;
        g_betterLootingRules.lootAmmo = on;
        g_betterLootingRules.lootBlueprints = on;
        g_betterLootingRules.lootBooks = on;
        g_betterLootingRules.lootRobotics = on;
        g_betterLootingRules.lootNarcotics = on;
        g_betterLootingRules.lootSeveredLimbs = on;
        g_betterLootingRules.lootOther = on;
    }
}


// ============================================================
// Настройки: две панели бок о бок
//
// Вкладок нет намеренно. Пресеты правят и числа, и галочки сразу, а на
// вкладках было видно только половину: нажал «Профиль: деньги» - и гадай,
// что там переключилось в категориях.
//
// ПОЧЕМУ ДВЕ ПАНЕЛИ, А НЕ ОДНО ОКНО С ДВУМЯ КОЛОНКАМИ. Сначала я сделал
// именно колонки: внешнее окно и внутри две панели через перегрузку
// createDatapanel(имя, родительский виджет, прокрутка) - ту же, которой
// игра наполняет вкладки своего окна настроек. Обе панели легли поверх
// друг друга в левом верхнем углу, и ни setRealPosition у их виджетов,
// ни порядок вызовов этого не меняли: судя по поведению, родительский
// виджет там задаёт не положение. Копать дальше в чужой скомпилированный
// код дороже, чем поставить две обычные панели рядом.
//
// Открываются и закрываются вместе, крестик на любой закрывает обе.
// Положение считается в пикселях от размера окна игры, поэтому не зависит
// от того, как панель решит себя растянуть под содержимое.
// ============================================================

class BetterLootingPanel
{
public:
    static BetterLootingPanel* getSingletonPtr()
    {
        static boost::scoped_ptr<BetterLootingPanel> singleton(
            new BetterLootingPanel());
        return singleton.get();
    }

    void toggle()
    {
        if (!_settings)
            create();
        if (!_settings || !_categories)
            return;

        if (_settings->isVisible())
        {
            hide();
        }
        else
        {
            syncProxies();
            refresh();
            _settings->show(true);
            _categories->show(true);
            // Расставляем только при открытии: число строк от пресетов не
            // меняется, а на каждом нажатии пара прыгала бы обратно в
            // центр, отменяя то, куда игрок их перетащил.
            layout();
            raise();
        }
    }

    // Пресеты из MCM: те же наборы, что у кнопок панели, и сразу в ini -
    // у страницы MCM кнопки «Сохранить» нет. Открытая панель обновится.
    void applyPresetFromMcm(int which)
    {
        switch (which)
        {
        case 0: onPresetBalanced(NULL); break;
        case 1: onPresetMoney(NULL); break;
        case 2: onPresetResources(NULL); break;
        case 3: onAll(NULL); break;
        case 4: onNone(NULL); break;
        default: return;
        }
        BetterLootingSaveRules();
    }

private:
    BetterLootingPanel()
        : _settings(NULL), _categories(NULL)
    {
    }

    void create()
    {
        // Ширина и высота - доли экрана. Высота под самую длинную панель:
        // в категориях 13 галочек и две кнопки.
        _settings = gui->createDatapanel(0.16f, 0.16f, 0.30f, 0.56f,
                                         false, "Window", true);
        _categories = gui->createDatapanel(0.16f, 0.48f, 0.30f, 0.56f,
                                           false, "Window", true);

        if (!_settings || !_categories)
        {
            ErrorLog("BetterLooting: PANEL createDatapanel failed");
            return;
        }

        _settings->setCaption(Tr("Better Looting"));
        _settings->setPanelName("BetterLooting");

        _categories->setCaption(Tr("Better Looting"));
        _categories->setPanelName("BetterLootingCategories");

        // Место под полосу вкладок панель держит всегда, добавляли мы их
        // или нет: между заголовком и первой строкой зияла пустота
        // примерно в две строки. Погасить её showTabs(false) нельзя -
        // игра на этом вылетает (проверено, RE_Kenshi_log 21.09.2026).
        // Поэтому вместо пустоты кладём по одной вкладке с названием
        // раздела: полоса всё равно нарисуется, пусть работает подписью.
        _settings->showTabs(true);
        _settings->addTab(CATEGORY, Tr("Looting"), "");
        _settings->changeCategory(CATEGORY);

        _categories->showTabs(true);
        _categories->addTab(CATEGORY, Tr("Categories"), "");
        _categories->changeCategory(CATEGORY);

        // Крестик на любой из панелей закрывает обе и записывает настройки:
        // галочки правят структуру правил напрямую.
        _settings->setCloseCallback(
            MyGUI::newDelegate(this, &BetterLootingPanel::onClose));
        _categories->setCloseCallback(
            MyGUI::newDelegate(this, &BetterLootingPanel::onClose));

        syncProxies();
        refresh();
        _settings->show(false);
        _categories->show(false);
    }

    // Ставим панели вплотную, парой по центру экрана. Размеры берём
    // фактические, уже после построения строк: панель сама решает,
    // насколько растянуться, и гадать об этом заранее бессмысленно.
    void layout()
    {
        MyGUI::Widget* left = _settings->getWidget();
        MyGUI::Widget* right = _categories->getWidget();
        if (!left || !right)
            return;

        const MyGUI::IntSize view =
            MyGUI::RenderManager::getInstance().getViewSize();
        if (view.width <= 0 || view.height <= 0)
            return;

        const int gap = 4;
        const int total = left->getWidth() + gap + right->getWidth();

        int height = left->getHeight();
        if (right->getHeight() > height)
            height = right->getHeight();

        int x = (view.width - total) / 2;
        int y = (view.height - height) / 2;
        if (x < 0) x = 0;
        if (y < 0) y = 0;

        left->setPosition(x, y);
        right->setPosition(x + left->getWidth() + gap, y);
    }

    void raise()
    {
        MyGUI::LayerManager* layers = MyGUI::LayerManager::getInstancePtr();
        if (!layers)
            return;
        layers->upLayerItem(_settings->getWidget());
        layers->upLayerItem(_categories->getWidget());
    }

    void hide()
    {
        applyProxies();
        BetterLootingSaveRules();
        _settings->show(false);
        _categories->show(false);
    }

    void syncProxies()
    {
        g_minValueProxy =
            static_cast<float>(g_betterLootingRules.minValue);
    }

    void applyProxies()
    {
        g_betterLootingRules.minValue =
            static_cast<int>(g_minValueProxy + 0.5f);
    }

    void tip(MyGUI::Widget* widget, const char* text)
    {
        if (!widget)
            return;
        ToolTip* tooltip = gui->getToolTip();
        if (tooltip)
            tooltip->setup(widget, std::string(text));
    }

    void refresh()
    {
        if (!_settings || !_categories)
            return;

        // Строк на экран: чем больше число, тем ниже строка. Длиннее
        // всех панель категорий - 13 галочек плюс две кнопки, и при 28
        // нижняя кнопка не влезала.
        _settings->setLineSpacing(34.0f);
        _categories->setLineSpacing(34.0f);

        _settings->clearPage(CATEGORY);
        _categories->clearPage(CATEGORY);

        buildSettings();
        buildCategories();
    }

    void buildSettings()
    {
        DatapanelGUI* p = _settings;

        DataPanelLine_SliderEditable* slider =
            p->setLineSliderEditable(
                Tr("Loot radius (m)"), CATEGORY, true,
                1.0f, 50.0f, &g_betterLootingRules.lootRadius);
        slider->setPrecision(2);
        tip(slider->nameText,
            Tr("How far around the character the mod looks for loot."));

        slider = p->setLineSliderEditable(
            Tr("Minimum value"), CATEGORY, true,
            0.0f, 5000.0f, &g_minValueProxy);
        slider->setPrecision(0);
        tip(slider->nameText,
            Tr("Items cheaper than this are left where they are."));

        slider = p->setLineSliderEditable(
            Tr("Minimum value per kg"), CATEGORY, true,
            0.0f, 2000.0f, &g_betterLootingRules.minValuePerKg);
        slider->setPrecision(0);
        tip(slider->nameText,
            Tr("Cuts off heavy items with a poor value to weight ratio."));

        g_hotkeyIndexProxy =
            HotkeyIndexOf(g_betterLootingRules.activationVirtualKey);

        DataPanelLine_DropBox* drop =
            p->setLineDropBox(
                Tr("Looting key"), CATEGORY,
                &g_hotkeyIndexProxy, false, 0.45f);
        for (int i = 0; i < HOTKEY_COUNT; ++i)
            drop->addAValue(HOTKEYS[i].name, i);
        drop->setSelectedValue(g_hotkeyIndexProxy);
        if (drop->getComboBox() != NULL)
            drop->getComboBox()->eventComboAccept +=
                MyGUI::newDelegate(&OnHotkeyPicked);
        tip(drop->getComboBox(),
            Tr("Key that collects loot around the selected character."));

        p->addSpace(CATEGORY, 0.25f);

        DataPanelLine_CheckBox* check =
            p->setLineCheckbox(Tr("Allow stolen items"),
                               &g_betterLootingRules.allowStolen, CATEGORY);
        tip(check->getTextBox(),
            Tr("Pick up goods that still belong to someone else."));

        check = p->setLineCheckbox(Tr("Best items first"),
                                   &g_betterLootingRules.bestItemsFirst,
                                   CATEGORY);
        tip(check->getTextBox(),
            Tr("Sort by value, so a full backpack holds the best finds."));

        check = p->setLineCheckbox(Tr("Prefer backpack"),
                                   &g_betterLootingRules.preferBackpack,
                                   CATEGORY);
        tip(check->getTextBox(),
            Tr("Fill the backpack before the character's own inventory."));

        p->addSpace(CATEGORY, 0.25f);

        DataPanelLine_Button* button =
            p->setLineButton("", Tr("Deposit everything"), CATEGORY);
        button->button->eventMouseButtonClick +=
            MyGUI::newDelegate(this, &BetterLootingPanel::onStoreAll);
        tip(button->button,
            Tr("Move everything that matches the rules into nearby storage."));

        button = p->setLineButton("", Tr("Save"), CATEGORY);
        button->button->eventMouseButtonClick +=
            MyGUI::newDelegate(this, &BetterLootingPanel::onSave);

        button = p->setLineButton("", Tr("Restore defaults"), CATEGORY);
        button->button->eventMouseButtonClick +=
            MyGUI::newDelegate(this, &BetterLootingPanel::onDefaults);

        p->addSpace(CATEGORY, 0.25f);

        // Профили правят и числа, и галочки. Стоят слева, а результат
        // виден справа - ради этого обе панели и открыты разом.
        button = p->setLineButton("", Tr("Profile: balanced"), CATEGORY);
        button->button->eventMouseButtonClick +=
            MyGUI::newDelegate(this, &BetterLootingPanel::onPresetBalanced);

        button = p->setLineButton("", Tr("Profile: money"), CATEGORY);
        button->button->eventMouseButtonClick +=
            MyGUI::newDelegate(this, &BetterLootingPanel::onPresetMoney);

        button = p->setLineButton("", Tr("Profile: resources"), CATEGORY);
        button->button->eventMouseButtonClick +=
            MyGUI::newDelegate(this, &BetterLootingPanel::onPresetResources);
    }

    void buildCategories()
    {
        DatapanelGUI* p = _categories;

        addCategory(p, Tr("Weapons"), &g_betterLootingRules.lootWeapons);
        addCategory(p, Tr("Armour"), &g_betterLootingRules.lootArmour);
        addCategory(p, Tr("Materials and resources"),
                    &g_betterLootingRules.lootMaterials);
        addCategory(p, Tr("Food"), &g_betterLootingRules.lootFood);
        addCategory(p, Tr("Medicine"), &g_betterLootingRules.lootMedicine);
        addCategory(p, Tr("Tools"), &g_betterLootingRules.lootTools);
        addCategory(p, Tr("Ammo"), &g_betterLootingRules.lootAmmo);
        addCategory(p, Tr("Blueprints"), &g_betterLootingRules.lootBlueprints);
        addCategory(p, Tr("Books"), &g_betterLootingRules.lootBooks);
        addCategory(p, Tr("Robotics"), &g_betterLootingRules.lootRobotics);
        addCategory(p, Tr("Narcotics"), &g_betterLootingRules.lootNarcotics);
        addCategory(p, Tr("Severed limbs"),
                    &g_betterLootingRules.lootSeveredLimbs);
        addCategory(p, Tr("Other"), &g_betterLootingRules.lootOther);

        p->addSpace(CATEGORY, 0.25f);

        DataPanelLine_Button* button =
            p->setLineButton("", Tr("All"), CATEGORY);
        button->button->eventMouseButtonClick +=
            MyGUI::newDelegate(this, &BetterLootingPanel::onAll);

        button = p->setLineButton("", Tr("None"), CATEGORY);
        button->button->eventMouseButtonClick +=
            MyGUI::newDelegate(this, &BetterLootingPanel::onNone);
    }

    void addCategory(DatapanelGUI* p, const char* caption, bool* value)
    {
        p->setLineCheckbox(caption, value, CATEGORY);
    }

    // --- обработчики ---

    void onClose(MyGUI::Window* sender, const std::string& name)
    {
        hide();
    }

    void onSave(MyGUI::Widget* sender)
    {
        applyProxies();
        BetterLootingSaveRules();
    }

    void onStoreAll(MyGUI::Widget* sender)
    {
        BetterLootingRequestStoreAll();
    }

    void onDefaults(MyGUI::Widget* sender)
    {
        g_betterLootingRules.lootRadius = 20.0f;
        g_betterLootingRules.minValue = 0;
        g_betterLootingRules.minValuePerKg = 0.0f;
        g_betterLootingRules.allowStolen = true;
        SetAllCategories(true);
        g_betterLootingRules.bestItemsFirst = true;
        g_betterLootingRules.preferBackpack = true;
        g_betterLootingRules.activationVirtualKey = VK_INSERT;

        syncProxies();
        BetterLootingSaveRules();
        refresh();
    }

    void onAll(MyGUI::Widget* sender)
    {
        SetAllCategories(true);
        refresh();
    }

    void onNone(MyGUI::Widget* sender)
    {
        SetAllCategories(false);
        refresh();
    }

    void applyPreset(float radius, int minValue, float minValuePerKg)
    {
        g_betterLootingRules.lootRadius = radius;
        g_betterLootingRules.minValue = minValue;
        g_betterLootingRules.minValuePerKg = minValuePerKg;
        syncProxies();
    }

    void onPresetBalanced(MyGUI::Widget* sender)
    {
        applyPreset(20.0f, 500, 300.0f);
        SetAllCategories(true);
        g_betterLootingRules.lootNarcotics = false;
        g_betterLootingRules.lootSeveredLimbs = false;
        refresh();
    }

    void onPresetMoney(MyGUI::Widget* sender)
    {
        applyPreset(25.0f, 1000, 500.0f);
        SetAllCategories(false);
        g_betterLootingRules.lootWeapons = true;
        g_betterLootingRules.lootArmour = true;
        g_betterLootingRules.lootBlueprints = true;
        g_betterLootingRules.lootBooks = true;
        g_betterLootingRules.lootRobotics = true;
        g_betterLootingRules.lootNarcotics = true;
        g_betterLootingRules.lootOther = true;
        refresh();
    }

    void onPresetResources(MyGUI::Widget* sender)
    {
        applyPreset(30.0f, 0, 0.0f);
        SetAllCategories(false);
        g_betterLootingRules.lootMaterials = true;
        g_betterLootingRules.lootFood = true;
        g_betterLootingRules.lootMedicine = true;
        g_betterLootingRules.lootTools = true;
        g_betterLootingRules.lootAmmo = true;
        g_betterLootingRules.lootRobotics = true;
        refresh();
    }

    // Страница у каждой панели одна: вкладок нет.
    static const int CATEGORY = 0;

    DatapanelGUI* _settings;
    DatapanelGUI* _categories;
};


// ============================================================
// Горячая клавиша
//
// Опрос идёт из покадрового обновления главной панели, то есть из игрового
// потока. Это принципиально: MyGUI не потокобезопасен, и прежнее окно
// открывалось из собственного потока только потому, что было не на MyGUI.
// ============================================================

namespace
{
    bool g_hotkeyWasDown = false;

    void (*g_origMainBarUpdate)(MainBarGUI* thisptr) = NULL;

    void MainBarUpdate_hook(MainBarGUI* thisptr)
    {
        g_origMainBarUpdate(thisptr);

        const bool down =
            (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0 &&
            (GetAsyncKeyState('L') & 0x8000) != 0;

        // Если фокус в поле ввода - буква принадлежит ему, а не нам.
        // Иначе переименование отряда или сохранение с буквой L в имени
        // открывало бы окно настроек посреди набора текста.
        const bool typing =
            MyGUI::InputManager::getInstance().getKeyFocusWidget() != NULL;

        if (down && !g_hotkeyWasDown && !typing)
            BetterLootingPanel::getSingletonPtr()->toggle();

        g_hotkeyWasDown = down;
    }
}


void BetterLootingApplyPreset(int which)
{
    BetterLootingPanel::getSingletonPtr()->applyPresetFromMcm(which);
}


void BetterLootingPanelInstallHooks()
{
    if (KenshiLib::SUCCESS !=
        KenshiLib::AddHook(
            KenshiLib::GetRealAddress(&MainBarGUI::_NV_update),
            MainBarUpdate_hook,
            &g_origMainBarUpdate))
    {
        ErrorLog("BetterLooting: PANEL hook MainBarGUI::_NV_update failed");
    }
}
