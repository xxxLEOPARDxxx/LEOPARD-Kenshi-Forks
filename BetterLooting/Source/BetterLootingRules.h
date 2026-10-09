#pragma once


struct BetterLootingRules
{
    float lootRadius;
    int minValue;
    float minValuePerKg;
    bool allowStolen;

    bool lootWeapons;
    bool lootArmour;
    bool lootFood;
    bool lootMedicine;
    bool lootTools;
    bool lootBlueprints;
    bool lootAmmo;
    bool lootRobotics;
    bool lootBooks;
    bool lootNarcotics;
    bool lootSeveredLimbs;
    bool lootOther;

    bool bestItemsFirst;
    bool preferBackpack;
    int activationVirtualKey;
    bool lootMaterials;
};


// v100 не знает scoped enum. Имя типа и запись Enum::VALUE сохраняются,
// см. fix_bl_vc10.py.
enum BetterLootingDecision
{
    LOOT,
    SKIP_CATEGORY,
    SKIP_STOLEN,
    SKIP_MIN_VALUE,
    SKIP_VALUE_PER_KG
};


// Определение уехало в BetterLooting.cpp. Здесь был `static`, и пока
// файл мода был один, это сходило с рук. С появлением второго .cpp
// (окно настроек) каждый получил бы собственную копию структуры: панель
// правила бы одну, а сбор смотрел в другую.
extern BetterLootingRules g_betterLootingRules;


inline bool IsAllowedLootCategory(
    int itemTypeValue,
    int itemFunctionValue,
    bool isResource
)
{
    switch (itemFunctionValue)
    {
        case ITEM_WEAPON:
            return g_betterLootingRules.lootWeapons;

        case ITEM_CLOTHING:
            return g_betterLootingRules.lootArmour;

        case ITEM_FOOD:
        case ITEM_FOOD_RESTRICTED:
            return g_betterLootingRules.lootFood;

        case ITEM_FIRSTAID:
        case ITEM_MEDRIGGING:
            return g_betterLootingRules.lootMedicine;

        case ITEM_TOOL:
            return g_betterLootingRules.lootTools;

        case ITEM_BLUEPRINT:
            return g_betterLootingRules.lootBlueprints;

        case ITEM_AMMO:
            return g_betterLootingRules.lootAmmo;

        case ITEM_ROBOTREPAIR:
            return g_betterLootingRules.lootRobotics;

        case ITEM_BOOK:
            return g_betterLootingRules.lootBooks;

        case ITEM_NARCOTIC:
            return g_betterLootingRules.lootNarcotics;

        case ITEM_SEVERED_LIMB:
            return g_betterLootingRules.lootSeveredLimbs;

        default:
            break;
    }


    // Explicit semantic categories win over resource context.
    // This prevents food marked by Kenshi as a resource from
    // being accepted by the Materials tick.
    if (isResource)
    {
        return g_betterLootingRules.lootMaterials;
    }


    switch (itemTypeValue)
    {
        case WEAPON:
        case CROSSBOW:
            return g_betterLootingRules.lootWeapons;

        case ARMOUR:
            return g_betterLootingRules.lootArmour;

        case BLUEPRINT:
            return g_betterLootingRules.lootBlueprints;

        case LIMB_REPLACEMENT:
            return g_betterLootingRules.lootRobotics;

        default:
            return g_betterLootingRules.lootOther;
    }
}


inline BetterLootingDecision EvaluateLootDecision(
    int valueSingle,
    float weightSingle,
    bool stolen,
    int itemTypeValue,
    int itemFunctionValue
,
    bool isResource
)
{
    if (
        !IsAllowedLootCategory(
            itemTypeValue,
            itemFunctionValue,
            isResource
        )
    )
    {
        return BetterLootingDecision::SKIP_CATEGORY;
    }

    if (
        stolen &&
        !g_betterLootingRules.allowStolen
    )
    {
        return BetterLootingDecision::SKIP_STOLEN;
    }

    if (
        valueSingle <
        g_betterLootingRules.minValue
    )
    {
        return BetterLootingDecision::SKIP_MIN_VALUE;
    }

    if (weightSingle > 0.001f)
    {
        const float valuePerKg =
            static_cast<float>(valueSingle) /
            weightSingle;

        if (
            valuePerKg <
            g_betterLootingRules.minValuePerKg
        )
        {
            return BetterLootingDecision::SKIP_VALUE_PER_KG;
        }
    }

    return BetterLootingDecision::LOOT;
}


inline const char* BetterLootingDecisionName(
    BetterLootingDecision decision
)
{
    switch (decision)
    {
        case BetterLootingDecision::LOOT:
            return "LOOT";

        case BetterLootingDecision::SKIP_CATEGORY:
            return "SKIP_CATEGORY";

        case BetterLootingDecision::SKIP_STOLEN:
            return "SKIP_STOLEN";

        case BetterLootingDecision::SKIP_MIN_VALUE:
            return "SKIP_MIN_VALUE";

        case BetterLootingDecision::SKIP_VALUE_PER_KG:
            return "SKIP_VALUE_PER_KG";

        default:
            return "UNKNOWN";
    }
}