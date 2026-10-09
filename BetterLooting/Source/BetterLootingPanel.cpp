// Пресеты BetterLooting для страницы MCM.
//
// Раньше здесь было окно настроек на интерфейсе игры (SHIFT+L): две панели
// с ползунками, галочками категорий, профилями и кнопкой «Разложить всё».
// С 04.10.2026 все настройки - во вкладке MCM окна «Настройки», и окно с
// горячей клавишей убраны по просьбе пользователя. Остались только наборы
// профилей - теми же числами, что были у кнопок окна. Прежний файл -
// BetterLootingPanel.cpp.prenopanel.

#include "BetterLootingPanel.h"

// BetterLootingRules.h пользуется перечислениями предметов из игры,
// но сам их не подключает.
#include <kenshi/Item.h>
#include "BetterLootingRules.h"


namespace
{
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

    void ApplyNumbers(float radius, int minValue, float minValuePerKg)
    {
        g_betterLootingRules.lootRadius = radius;
        g_betterLootingRules.minValue = minValue;
        g_betterLootingRules.minValuePerKg = minValuePerKg;
    }
}


void BetterLootingApplyPreset(int which)
{
    switch (which)
    {
    case 0:     // баланс: всё, кроме наркотиков и конечностей
        ApplyNumbers(20.0f, 500, 300.0f);
        SetAllCategories(true);
        g_betterLootingRules.lootNarcotics = false;
        g_betterLootingRules.lootSeveredLimbs = false;
        break;

    case 1:     // деньги: дорогое и лёгкое
        ApplyNumbers(25.0f, 1000, 500.0f);
        SetAllCategories(false);
        g_betterLootingRules.lootWeapons = true;
        g_betterLootingRules.lootArmour = true;
        g_betterLootingRules.lootBlueprints = true;
        g_betterLootingRules.lootBooks = true;
        g_betterLootingRules.lootRobotics = true;
        g_betterLootingRules.lootNarcotics = true;
        g_betterLootingRules.lootOther = true;
        break;

    case 2:     // ресурсы: расходники, любая цена
        ApplyNumbers(30.0f, 0, 0.0f);
        SetAllCategories(false);
        g_betterLootingRules.lootMaterials = true;
        g_betterLootingRules.lootFood = true;
        g_betterLootingRules.lootMedicine = true;
        g_betterLootingRules.lootTools = true;
        g_betterLootingRules.lootAmmo = true;
        g_betterLootingRules.lootRobotics = true;
        break;

    case 3:
        SetAllCategories(true);
        break;

    case 4:
        SetAllCategories(false);
        break;

    default:
        return;
    }

    BetterLootingSaveRules();
}
