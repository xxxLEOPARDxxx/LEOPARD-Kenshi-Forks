#define BOOST_ALL_NO_LIB
#define BOOST_SYSTEM_NO_LIB
#define BOOST_ERROR_CODE_HEADER_ONLY

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <Windows.h>
#include <CommCtrl.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <algorithm>

#include <Debug.h>
#include <cstring>

// Журнал. Автор писал всё подряд через ErrorLog: на каждое открытие
// инвентаря - строка «Error … CLASS source=…». Теперь сбои (FAIL, ERROR,
// fault, failed) идут в журнал всегда, остальное - только при [Debug] Log=1
// в BetterLooting.ini (или «Подробный журнал» в MCM).
static bool g_betterLootingDebugLog = false;

static bool BlIsProblem(const char* message)
{
    return std::strstr(message, "FAIL") != NULL
        || std::strstr(message, "ERROR") != NULL
        || std::strstr(message, "fault") != NULL
        || std::strstr(message, "failed") != NULL;
}

static void BlTrace(const char* message)
{
    if (message == NULL)
        return;
    if (BlIsProblem(message))
        ErrorLog(message);
    else if (g_betterLootingDebugLog)
        DebugLog(message);
}

static void BlTrace(const std::string& message)
{
    BlTrace(message.c_str());
}

#include <kenshi/Inventory.h>
#include <kenshi/Item.h>
#include <kenshi/gui/InventoryGUI.h>
#include <kenshi/gui/ForgottenGUI.h>
#include <kenshi/util/hand.h>

#include <core/Functions.h>

#include "BetterLootingRules.h"
#include "BetterLootingPanel.h"

// Определение здесь, а не в заголовке: файлов мода теперь два.
BetterLootingRules g_betterLootingRules =
{
    20.0f,
    500,
    300.0f,
    false,

    true,
    true,
    true,
    true,
    true,
    true,
    true,
    true,
    true,
    false,
    false,
    true,    true,
    true,
    45,
    true
};

// BL_FIX_STACK_TRANSFER_ONE_SHOT_ADDARG1_NO_EQUIPMENT
// BL_FIX_DESTINATION_AWARE_UNIT_TRANSFER_EXACT_SOURCE_ROLLBACK
// BL_FIX_SELECTIVE_AUTODEPOSIT_GETEXCESSLOOT
// BL_FEATURE_STORE_ALL_SHIFT_P_SAFE_ROUTING
// BL_UI_POLISH_DEPOSIT_EVERYTHING_HEADER_FLAGS
// BL_UI_REFINED_PASS2_TYPOGRAPHY_SPACING_HIERARCHY
// BL_UI_FINAL_SPACING_STATUS_BEHAVIOR_HELP
// BL_UI_FIX_FOOTER_PROFILE_REAL_SPACING
// BL_FIX_SINGLE_INSERT_COMPLETE_PASS_PREFLIGHT_OR_POSITION
// BL_FIX_RUNTIME_BACKPACK_REJECT_FALLBACK_TO_MAIN
// BL_FIX_STOLEN_OWNER_AUTODEPOSIT_KEY_ENGINE_PROBE
// BL_FIX_STORE_ALL_SPECIALIZED_RESOURCE_TARGET_DISCOVERY
// BL_FIX_PLAYER_DESTINATION_ORDER_AND_EMERGENCY_STORAGE_FALLBACK
// BL_FIX_STORAGE_GENERAL_FIRST_RESOURCE_FALLBACK_PLAYER_ROLLBACK
// BL_FIX_STORE_ALL_ORIGIN_PROVENANCE_FIRST
// BL_FIX_STRICT_WORLD_RADIUS_AND_ORIGIN_GUARD
// BL_FIX_STORE_RADIUS_SPLIT_FAIL_CLOSED_V5
// BL_FIX_INSERT_LOOT_EQUIPPED_SOURCE_ACCOUNTING_V6
// BL_FIX_INSERT_BODY_THEFT_POLICY_AND_STOLEN_OWNER_V7
// BL_FIX_INSERT_BODY_THEFT_NATIVE_NOTIFY_V8
// BL_FIX_INSERT_BODY_BATCH_INVARIANT_V9

#include <kenshi/GameWorld.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/Globals.h>
#include <kenshi/RootObject.h>
#include <kenshi/Character.h>
#include <unordered_set>
#include <kenshi/Building/Building.h>
#include <kenshi/Faction.h>

// В библиотеке VC10 нет strtof из C99. strtod есть, и его точности здесь
// с запасом: значение всё равно кладётся во float. См. fix_bl_vc10.py.
#if defined(_MSC_VER) && _MSC_VER < 1700
static inline float strtof(const char* text, char** end)
{
    return static_cast<float>(strtod(text, end));
}
#endif


// v100 не знает scoped enum, см. fix_bl_vc10.py.
enum InventoryKind
{
    UNKNOWN,
    PLAYER_MAIN,
    PLAYER_BACKPACK,
    CHARACTER_TARGET,
    CONTAINER_TARGET
};


typedef InventoryGUI* (*ShowInventoryFn)(
    ForgottenGUI*,
    const hand&,
    bool,
    float,
    float
);


typedef InventoryGUI* (*ShowSimpleInventoryFn)(
    ForgottenGUI*,
    const hand&
);


static ShowInventoryFn g_origShowInventory = NULL;

static ShowSimpleInventoryFn
    g_origShowInventoryNPC = NULL;

static ShowSimpleInventoryFn
    g_origShowInventoryBuilding = NULL;

static ShowSimpleInventoryFn
    g_origShowTraderInventory = NULL;


static InventoryGUI* g_playerMainWindow = NULL;
static InventoryGUI* g_playerBackpackWindow = NULL;


static InventoryGUI* SelectLootDestination()
{
    if (g_betterLootingRules.preferBackpack)
    {
        if (g_playerBackpackWindow)
            return g_playerBackpackWindow;

        if (g_playerMainWindow)
            return g_playerMainWindow;
    }
    else
    {
        if (g_playerMainWindow)
            return g_playerMainWindow;

        if (g_playerBackpackWindow)
            return g_playerBackpackWindow;
    }

    return NULL;
}

static const char* LootDestinationName(
    InventoryGUI* destination
)
{
    if (
        destination &&
        destination == g_playerBackpackWindow
    )
    {
        return "PLAYER_BACKPACK";
    }

    if (
        destination &&
        destination == g_playerMainWindow
    )
    {
        return "PLAYER_MAIN";
    }

    return "NONE";
}

static const char* InventoryKindName(
    InventoryKind kind
)
{
    switch (kind)
    {
        case InventoryKind::PLAYER_MAIN:
            return "PLAYER_MAIN";

        case InventoryKind::PLAYER_BACKPACK:
            return "PLAYER_BACKPACK";

        case InventoryKind::CHARACTER_TARGET:
            return "CHARACTER_TARGET";

        case InventoryKind::CONTAINER_TARGET:
            return "CONTAINER_TARGET";

        default:
            return "UNKNOWN";
    }
}


static InventoryKind ClassifyInventory(
    InventoryGUI* guiWindow,
    const hand& owner
)
{
    if (!guiWindow)
        return InventoryKind::UNKNOWN;

    Inventory* inventory =
        guiWindow->getInventory();

    if (!inventory)
        return InventoryKind::UNKNOWN;

    Character* character =
        guiWindow->getCallbackCharacter();

    if (character)
    {
        if (character->isPlayerCharacter())
        {
            if (inventory->isAContainer())
                return InventoryKind::PLAYER_BACKPACK;

            return InventoryKind::PLAYER_MAIN;
        }

        return InventoryKind::CHARACTER_TARGET;
    }

    if (owner.getBuilding())
        return InventoryKind::CONTAINER_TARGET;

    if (inventory->isAContainer())
        return InventoryKind::CONTAINER_TARGET;

    return InventoryKind::UNKNOWN;
}





struct BetterLootingItemLocation
{
    InventorySection* section;
    int x;
    int y;
    bool found;
};


struct BetterLootingCandidate
{
    Item* item;
    InventorySection* section;
    int x;
    int y;
};


static bool GetSectionGridSize(
    InventorySection* section,
    int& width,
    int& height
)
{
    width = 0;
    height = 0;

    if (!section)
        return false;

    const unsigned char* base =
        reinterpret_cast<const unsigned char*>(section);

    width =
        *reinterpret_cast<const int*>(base + 0x30);

    height =
        *reinterpret_cast<const int*>(base + 0x34);

    if (
        width <= 0 ||
        height <= 0 ||
        width > 64 ||
        height > 64
    )
    {
        char message[256];

        sprintf_s(
            message,
            "BetterLooting: GRID_INVALID section=%p width=%d height=%d",
            section,
            width,
            height
        );

        BlTrace(message);

        return false;
    }

    return true;
}


static bool CandidateAlreadyAdded(
    const std::vector<BetterLootingCandidate>& candidates,
    Item* item
)
{
    for (
        std::size_t i = 0;
        i < candidates.size();
        ++i
    )
    {
        if (candidates[i].item == item)
            return true;
    }

    return false;
}


static std::vector<BetterLootingCandidate>
EnumerateInventoryCandidates(
    Inventory* inventory
)
{
    std::vector<BetterLootingCandidate> result;

    if (!inventory)
        return result;

    const int expectedItems =
        inventory->getNumItems();

    if (expectedItems > 0)
    {
        result.reserve(
            static_cast<std::size_t>(expectedItems)
        );
    }

    const lektor<InventorySection*>& sections =
        inventory->getAllSections();

    for (
        uint32_t sectionIndex = 0;
        sectionIndex <
            static_cast<uint32_t>(sections.size());
        ++sectionIndex
    )
    {
        InventorySection* section =
            sections[sectionIndex];

        if (!section)
            continue;

        int width = 0;
        int height = 0;

        if (!GetSectionGridSize(section, width, height))
            continue;

        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                Item* item =
                    section->getItemAt(x, y);

                if (!item)
                    continue;

                if (CandidateAlreadyAdded(result, item))
                    continue;

                BetterLootingCandidate candidate;

                candidate.item = item;
                candidate.section = section;
                candidate.x = x;
                candidate.y = y;

                result.push_back(candidate);
            }
        }
    }


    if (g_betterLootingRules.bestItemsFirst)
    {
        std::sort(
            result.begin(),
            result.end(),
            [](
                const BetterLootingCandidate& left,
                const BetterLootingCandidate& right
            ) -> bool
            {
                if (!left.item)
                    return false;

                if (!right.item)
                    return true;

                const int leftValue =
                    left.item->getValueSingle(true);

                const int rightValue =
                    right.item->getValueSingle(true);

                const float leftWeight =
                    left.item->getItemWeightSingle();

                const float rightWeight =
                    right.item->getItemWeightSingle();

                const float leftRatio =
                    leftWeight > 0.001f
                        ? static_cast<float>(leftValue) /
                            leftWeight
                        : static_cast<float>(leftValue) *
                            1000000.0f;

                const float rightRatio =
                    rightWeight > 0.001f
                        ? static_cast<float>(rightValue) /
                            rightWeight
                        : static_cast<float>(rightValue) *
                            1000000.0f;

                if (leftRatio != rightRatio)
                    return leftRatio > rightRatio;

                return leftValue > rightValue;
            }
        );
    }


    return result;
}

static BetterLootingItemLocation FindItemLocation(
    Inventory* inventory,
    Item* item
)
{
    BetterLootingItemLocation result = {
        nullptr,
        0,
        0,
        false
    };

    if (!inventory || !item)
        return result;

    const std::vector<BetterLootingCandidate> candidates =
        EnumerateInventoryCandidates(inventory);

    for (
        std::size_t i = 0;
        i < candidates.size();
        ++i
    )
    {
        if (candidates[i].item != item)
            continue;

        result.section = candidates[i].section;
        result.x = candidates[i].x;
        result.y = candidates[i].y;
        result.found = true;

        return result;
    }

    return result;
}

static int GetBetterLootingItemQuantity(
    Item* item
)
{
    if (!item)
    {
        return 0;
    }

    const unsigned char* base =
        reinterpret_cast<const unsigned char*>(
            item
        );

    return *reinterpret_cast<const int*>(
        base + 0x12C
    );
}


static Character*
GetBetterLootingLootableCharacterSource(
    Inventory* sourceInventory
)
{
    if (!sourceInventory)
    {
        return NULL;
    }


    Character* sourceCharacter =
        sourceInventory->
            getCallbackCharacter();


    if (
        !sourceCharacter ||
        sourceCharacter->isPlayerCharacter()
    )
    {
        return NULL;
    }


    if (
        !sourceCharacter->isDead() &&
        !sourceCharacter->isUnconcious()
    )
    {
        return NULL;
    }


    return sourceCharacter;
}


static int CountBetterLootingStolenQuantity(
    Inventory* inventory,
    GameData* itemData
)
{
    if (
        !inventory ||
        !itemData
    )
    {
        return 0;
    }


    int total = 0;


    const std::vector<BetterLootingCandidate> candidates =
        EnumerateInventoryCandidates(
            inventory
        );


    for (
        std::size_t i = 0;
        i < candidates.size();
        ++i
    )
    {
        Item* item =
            candidates[i].item;


        if (
            !item ||
            item->getGameData() !=
                itemData ||
            !item->isStolen(true)
        )
        {
            continue;
        }


        const int quantity =
            GetBetterLootingItemQuantity(
                item
            );


        if (
            quantity <= 0 ||
            quantity > 100000
        )
        {
            continue;
        }


        total += quantity;


        if (total > 100000000)
        {
            return 100000000;
        }
    }


    return total;
}


//
// BetterLooting selective auto-deposit protection.
//
// Kenshi asks Inventory::getExcessLoot() which items should be treated as
// disposable/excess loot. BetterLooting records only the item types and
// quantities that it successfully moved into a player inventory. The hook
// removes those protected stacks from the excess-loot result while leaving
// every unrelated item and every manual inventory action untouched.
//
// Protection is tracked per character + destination inventory + GameData.
// This prevents a protected quantity in the backpack from accidentally
// suppressing unrelated items of the same type in another inventory.
//

struct BetterLootingProtectedLoot
{
    hand characterHandle;
    Inventory* inventory;
    GameData* itemData;
    int baselineCount;
    int protectedQuantity;
};


static std::vector<BetterLootingProtectedLoot>
    g_betterLootingProtectedLoot;


//
// BL_FIX_STORE_ALL_ORIGIN_PROVENANCE_FIRST
//
// BetterLooting remembers where successfully looted units came from.
// STORE_ALL can then return each protected unit to its original PLAYER storage
// before considering other nearby GENERAL/RESOURCE storages.
//
// Provenance is tracked per PLAYER destination inventory + GameData +
// origin inventory + quantity. The origin inventory is never dereferenced
// during lookup; it is used only as an identity key and must also appear as a
// currently validated PLAYER storage target before it can be selected.
//

struct BetterLootingLootOrigin
{
    Inventory* playerInventory;
    GameData* itemData;
    Inventory* originInventory;
    int quantity;
};


static std::vector<BetterLootingLootOrigin>
    g_betterLootingLootOrigins;


static void RegisterBetterLootingLootOrigin(
    Inventory* playerInventory,
    GameData* itemData,
    Inventory* originInventory,
    int quantity
)
{
    if (
        !playerInventory ||
        !itemData ||
        !originInventory ||
        quantity <= 0
    )
    {
        return;
    }


    Character* character =
        playerInventory->
            getCallbackCharacter();


    if (
        !character ||
        !character->isPlayerCharacter()
    )
    {
        return;
    }


    for (
        std::size_t i = 0;
        i < g_betterLootingLootOrigins.size();
        ++i
    )
    {
        BetterLootingLootOrigin& entry =
            g_betterLootingLootOrigins[i];


        if (
            entry.playerInventory ==
                playerInventory &&
            entry.itemData ==
                itemData &&
            entry.originInventory ==
                originInventory
        )
        {
            entry.quantity +=
                quantity;


            char updateMessage[640];

            sprintf_s(
                updateMessage,
                "BetterLooting: ORIGIN_PROVENANCE_UPDATE playerInventory=%p gameData=%p originInventory=%p added=%d quantity=%d",
                playerInventory,
                itemData,
                originInventory,
                quantity,
                entry.quantity
            );

            BlTrace(
                updateMessage
            );

            return;
        }
    }


    BetterLootingLootOrigin entry;

    entry.playerInventory =
        playerInventory;

    entry.itemData =
        itemData;

    entry.originInventory =
        originInventory;

    entry.quantity =
        quantity;


    g_betterLootingLootOrigins.push_back(
        entry
    );


    char addMessage[640];

    sprintf_s(
        addMessage,
        "BetterLooting: ORIGIN_PROVENANCE_ADD playerInventory=%p gameData=%p originInventory=%p quantity=%d",
        playerInventory,
        itemData,
        originInventory,
        quantity
    );

    BlTrace(
        addMessage
    );
}


static Inventory*
GetBetterLootingPreferredOriginInventory(
    Inventory* playerInventory,
    GameData* itemData
)
{
    if (
        !playerInventory ||
        !itemData
    )
    {
        return NULL;
    }


    for (
        std::size_t i = 0;
        i < g_betterLootingLootOrigins.size();
        ++i
    )
    {
        const BetterLootingLootOrigin& entry =
            g_betterLootingLootOrigins[i];


        if (
            entry.playerInventory ==
                playerInventory &&
            entry.itemData ==
                itemData &&
            entry.originInventory &&
            entry.quantity > 0
        )
        {
            return
                entry.originInventory;
        }
    }


    return NULL;
}


static void ReleaseBetterLootingLootOriginForStore(
    Inventory* playerInventory,
    GameData* itemData,
    int quantity
)
{
    if (
        !playerInventory ||
        !itemData ||
        quantity <= 0
    )
    {
        return;
    }


    int remaining =
        quantity;


    for (
        std::size_t i = 0;
        i < g_betterLootingLootOrigins.size() &&
        remaining > 0;
        ++i
    )
    {
        BetterLootingLootOrigin& entry =
            g_betterLootingLootOrigins[i];


        if (
            entry.playerInventory !=
                playerInventory ||
            entry.itemData !=
                itemData ||
            entry.quantity <= 0
        )
        {
            continue;
        }


        const int released =
            entry.quantity < remaining
                ? entry.quantity
                : remaining;


        const int before =
            entry.quantity;


        entry.quantity -=
            released;

        remaining -=
            released;


        char releaseMessage[768];

        sprintf_s(
            releaseMessage,
            "BetterLooting: ORIGIN_PROVENANCE_RELEASE playerInventory=%p gameData=%p originInventory=%p before=%d released=%d after=%d",
            playerInventory,
            itemData,
            entry.originInventory,
            before,
            released,
            entry.quantity
        );

        BlTrace(
            releaseMessage
        );
    }
}


typedef bool (*InventoryGetExcessLootFn)(
    Inventory*,
    const hand&,
    lektor<Item*>&,
    bool
);


static InventoryGetExcessLootFn
    g_origInventoryGetExcessLoot = NULL;


static bool BetterLootingSameCharacterHandle(
    const hand& left,
    const hand& right
)
{
    Character* leftCharacter =
        left.getCharacter();

    Character* rightCharacter =
        right.getCharacter();


    if (
        leftCharacter &&
        rightCharacter
    )
    {
        return
            leftCharacter ==
            rightCharacter;
    }


    return left == right;
}


static int FindBetterLootingProtectionIndex(
    const hand& characterHandle,
    Inventory* inventory,
    GameData* itemData
)
{
    for (
        std::size_t i = 0;
        i < g_betterLootingProtectedLoot.size();
        ++i
    )
    {
        const BetterLootingProtectedLoot& entry =
            g_betterLootingProtectedLoot[i];


        if (
            entry.inventory == inventory &&
            entry.itemData == itemData &&
            BetterLootingSameCharacterHandle(
                entry.characterHandle,
                characterHandle
            )
        )
        {
            return static_cast<int>(i);
        }
    }


    return -1;
}


static int GetBetterLootingActiveProtectedQuantity(
    const hand& characterHandle,
    Inventory* inventory,
    GameData* itemData
)
{
    const int index =
        FindBetterLootingProtectionIndex(
            characterHandle,
            inventory,
            itemData
        );


    if (index < 0)
    {
        return 0;
    }


    BetterLootingProtectedLoot& entry =
        g_betterLootingProtectedLoot[
            static_cast<std::size_t>(index)
        ];


    const int currentCount =
        inventory
            ? inventory->countItems(itemData)
            : 0;


    const int quantityAboveBaseline =
        currentCount -
        entry.baselineCount;


    if (quantityAboveBaseline <= 0)
    {
        if (entry.protectedQuantity > 0)
        {
            char clearMessage[512];

            sprintf_s(
                clearMessage,
                "BetterLooting: AUTODEPOSIT_PROTECT_CLEAR inventory=%p gameData=%p baseline=%d current=%d protected=%d",
                inventory,
                itemData,
                entry.baselineCount,
                currentCount,
                entry.protectedQuantity
            );

            BlTrace(clearMessage);
        }


        entry.protectedQuantity = 0;

        return 0;
    }


    if (
        entry.protectedQuantity >
        quantityAboveBaseline
    )
    {
        entry.protectedQuantity =
            quantityAboveBaseline;
    }


    return entry.protectedQuantity;
}


static bool HasBetterLootingProtectionForInventory(
    const hand& characterHandle,
    Inventory* inventory
)
{
    for (
        std::size_t i = 0;
        i < g_betterLootingProtectedLoot.size();
        ++i
    )
    {
        const BetterLootingProtectedLoot& entry =
            g_betterLootingProtectedLoot[i];


        if (
            entry.inventory == inventory &&
            BetterLootingSameCharacterHandle(
                entry.characterHandle,
                characterHandle
            )
        )
        {
            const int currentCount =
                inventory->countItems(
                    entry.itemData
                );


            if (
                currentCount >
                entry.baselineCount &&
                entry.protectedQuantity > 0
            )
            {
                return true;
            }
        }
    }


    return false;
}


static void RegisterBetterLootingProtectedLoot(
    Inventory* destinationInventory,
    GameData* itemData,
    int quantity
)
{
    if (
        !destinationInventory ||
        !itemData ||
        quantity <= 0
    )
    {
        return;
    }


    Character* character =
        destinationInventory->
            getCallbackCharacter();


    if (
        !character ||
        !character->isPlayerCharacter()
    )
    {
        BlTrace(
            "BetterLooting: AUTODEPOSIT_PROTECT_SKIP destination has no player character"
        );

        return;
    }


    hand characterHandle(character);


    const int currentCount =
        destinationInventory->
            countItems(itemData);


    const int index =
        FindBetterLootingProtectionIndex(
            characterHandle,
            destinationInventory,
            itemData
        );


    if (index >= 0)
    {
        BetterLootingProtectedLoot& entry =
            g_betterLootingProtectedLoot[
                static_cast<std::size_t>(index)
            ];


        if (
            currentCount <=
            entry.baselineCount
        )
        {
            entry.baselineCount =
                currentCount -
                quantity;

            if (entry.baselineCount < 0)
            {
                entry.baselineCount = 0;
            }

            entry.protectedQuantity =
                quantity;
        }
        else
        {
            entry.protectedQuantity +=
                quantity;
        }


        char updateMessage[512];

        sprintf_s(
            updateMessage,
            "BetterLooting: AUTODEPOSIT_PROTECT_UPDATE character=%p inventory=%p gameData=%p baseline=%d current=%d protected=%d",
            character,
            destinationInventory,
            itemData,
            entry.baselineCount,
            currentCount,
            entry.protectedQuantity
        );

        BlTrace(updateMessage);

        return;
    }


    BetterLootingProtectedLoot entry;

    entry.characterHandle =
        characterHandle;

    entry.inventory =
        destinationInventory;

    entry.itemData =
        itemData;

    entry.baselineCount =
        currentCount -
        quantity;

    if (entry.baselineCount < 0)
    {
        entry.baselineCount = 0;
    }

    entry.protectedQuantity =
        quantity;


    g_betterLootingProtectedLoot.push_back(
        entry
    );


    char addMessage[512];

    sprintf_s(
        addMessage,
        "BetterLooting: AUTODEPOSIT_PROTECT_ADD character=%p inventory=%p gameData=%p baseline=%d current=%d protected=%d",
        character,
        destinationInventory,
        itemData,
        entry.baselineCount,
        currentCount,
        entry.protectedQuantity
    );

    BlTrace(addMessage);
}


static bool InventoryGetExcessLoot_hook(
    Inventory* self,
    const hand& me,
    lektor<Item*>& out,
    bool justAsking
)
{
    Character* character =
        me.getCharacter();


    if (
        !character &&
        self
    )
    {
        character =
            self->getCallbackCharacter();
    }


    if (
        !self ||
        !character ||
        !character->isPlayerCharacter()
    )
    {
        return g_origInventoryGetExcessLoot(
            self,
            me,
            out,
            justAsking
        );
    }


    hand characterHandle(character);


    const bool hasProtection =
        HasBetterLootingProtectionForInventory(
            characterHandle,
            self
        );


    if (!hasProtection)
    {
        return g_origInventoryGetExcessLoot(
            self,
            me,
            out,
            justAsking
        );
    }


    //
    // justAsking can allow the original function to return before filling
    // the output list. With active BetterLooting protection, force the
    // read-only enumeration path so the result can be filtered precisely.
    //

    const bool originalResult =
        g_origInventoryGetExcessLoot(
            self,
            me,
            out,
            false
        );


    const uint32_t originalCount =
        out.size();


    struct FilterBudget
    {
        GameData* itemData;
        int remainingQuantity;
    };


    std::vector<FilterBudget> budgets;


    for (
        std::size_t protectionIndex = 0;
        protectionIndex <
            g_betterLootingProtectedLoot.size();
        ++protectionIndex
    )
    {
        const BetterLootingProtectedLoot& entry =
            g_betterLootingProtectedLoot[
                protectionIndex
            ];


        if (
            entry.inventory != self ||
            !BetterLootingSameCharacterHandle(
                entry.characterHandle,
                characterHandle
            )
        )
        {
            continue;
        }


        const int activeQuantity =
            GetBetterLootingActiveProtectedQuantity(
                characterHandle,
                self,
                entry.itemData
            );


        if (activeQuantity <= 0)
        {
            continue;
        }


        FilterBudget budget;

        budget.itemData =
            entry.itemData;

        budget.remainingQuantity =
            activeQuantity;


        budgets.push_back(
            budget
        );
    }


    uint32_t writeIndex = 0;
    uint32_t blockedStacks = 0;
    int blockedQuantity = 0;


    for (
        uint32_t readIndex = 0;
        readIndex < originalCount;
        ++readIndex
    )
    {
        Item* item =
            out[readIndex];


        bool blockItem =
            false;


        if (item)
        {
            GameData* itemData =
                item->getGameData();


            for (
                std::size_t budgetIndex = 0;
                budgetIndex < budgets.size();
                ++budgetIndex
            )
            {
                FilterBudget& budget =
                    budgets[budgetIndex];


                if (
                    budget.itemData != itemData ||
                    budget.remainingQuantity <= 0
                )
                {
                    continue;
                }


                blockItem = true;

                ++blockedStacks;


                int stackQuantity =
                    GetBetterLootingItemQuantity(
                        item
                    );


                if (stackQuantity <= 0)
                {
                    stackQuantity = 1;
                }


                blockedQuantity +=
                    stackQuantity;


                budget.remainingQuantity -=
                    stackQuantity;


                if (budget.remainingQuantity < 0)
                {
                    budget.remainingQuantity = 0;
                }


                break;
            }
        }


        if (!blockItem)
        {
            out.stuff[writeIndex] =
                item;

            ++writeIndex;
        }
    }


    out.count =
        writeIndex;


    if (blockedStacks > 0)
    {
        char filterMessage[768];

        sprintf_s(
            filterMessage,
            "BetterLooting: AUTODEPOSIT_FILTER character=%p inventory=%p justAsking=%d originalResult=%d candidates=%u allowed=%u blockedStacks=%u blockedQuantity=%d",
            character,
            self,
            justAsking ? 1 : 0,
            originalResult ? 1 : 0,
            originalCount,
            out.size(),
            blockedStacks,
            blockedQuantity
        );

        BlTrace(filterMessage);
    }


    //
    // The filtered list is now authoritative. Returning false when empty
    // prevents the automatic storage job from being created for protected
    // BetterLooting items.
    //

    return out.size() > 0;
}

static bool g_betterLootingTransferFault = false;


struct BetterLootingDirectPlacement
{
    InventoryGUI* window;
    Inventory* inventory;
    InventorySection* section;
    int x;
    int y;
    int maxStack;
    bool equipmentSection;
    bool found;
};


static BetterLootingDirectPlacement
FindBetterLootingDirectPlacementInWindow(
    InventoryGUI* window,
    Item* item,
    GameData* itemData,
    int quantity,
    bool equipmentOnly
)
{
    BetterLootingDirectPlacement result = {
        nullptr,
        nullptr,
        nullptr,
        0,
        0,
        0,
        false,
        false
    };


    if (
        !window ||
        !item ||
        !itemData ||
        quantity <= 0
    )
    {
        return result;
    }


    Inventory* inventory =
        window->getInventory();


    if (!inventory)
    {
        return result;
    }


    lektor<InventorySection*>& sections =
        inventory->getAllSections();


    for (
        uint32_t i = 0;
        i < static_cast<uint32_t>(
            sections.size()
        );
        ++i
    )
    {
        InventorySection* section =
            sections[i];


        if (!section)
        {
            continue;
        }


        if (!section->getEnabled())
        {
            continue;
        }


        if (section->containerSlot)
        {
            continue;
        }


        if (
            equipmentOnly !=
            section->isAnEquippedItemSection
        )
        {
            continue;
        }


        if (
            !section->
                isLimitedSlotCompatible(item)
        )
        {
            continue;
        }


        const bool roomHint =
            section->
                hasRoomForItem(
                    itemData,
                    quantity
                );


        int x = 0;
        int y = 0;


        const bool positionOk =
            section->
                getValidInventoryPosition(
                    item,
                    x,
                    y
                );


        if (
            !roomHint &&
            !positionOk
        )
        {
            BlTrace(
                "BetterLooting: PLACEMENT_ENGINE_PROBE roomHint=0 positionOk=0"
            );
        }
        else if (
            !roomHint &&
            positionOk
        )
        {
            BlTrace(
                "BetterLooting: PLACEMENT_RECOVER positionOk=1 roomHint=0"
            );
        }


        int maxStack =
            item->isStackable(section);


        if (maxStack <= 0)
        {
            maxStack = 1;
        }


        result.window = window;
        result.inventory = inventory;
        result.section = section;
        result.x = x;
        result.y = y;
        result.maxStack = maxStack;
        result.equipmentSection =
            section->isAnEquippedItemSection;
        result.found = true;

        return result;
    }


    return result;
}


static BetterLootingDirectPlacement
FindBetterLootingDirectPlacement(
    InventoryGUI* preferredWindow,
    Item* item,
    GameData* itemData,
    int quantity
)
{
    BetterLootingDirectPlacement result =
        FindBetterLootingDirectPlacementInWindow(
            preferredWindow,
            item,
            itemData,
            quantity,
            false
        );


    if (result.found)
    {
        return result;
    }


    InventoryGUI* secondaryWindow =
        NULL;


    if (
        preferredWindow ==
            g_playerBackpackWindow
    )
    {
        secondaryWindow =
            g_playerMainWindow;
    }
    else if (
        preferredWindow ==
            g_playerMainWindow
    )
    {
        secondaryWindow =
            g_playerBackpackWindow;
    }


    if (
        secondaryWindow &&
        secondaryWindow !=
            preferredWindow
    )
    {
        result =
            FindBetterLootingDirectPlacementInWindow(
                secondaryWindow,
                item,
                itemData,
                quantity,
                false
            );


        if (result.found)
        {
            return result;
        }
    }


    return result;
}


//
// BL_FIX_PLAYER_DESTINATION_ORDER_AND_EMERGENCY_STORAGE_FALLBACK
//
// Mandatory loot destination order:
//
// preferBackpack = true:
//   1. PLAYER_BACKPACK
//   2. PLAYER_MAIN
//
// preferBackpack = false:
//   1. PLAYER_MAIN
//   2. PLAYER_BACKPACK
//
// If both reject the detached unit:
//   3. exact source section rollback
//
// Only if the exact source rollback itself fails while the unit remains
// detached and no attempted destination changed:
//   4. nearest compatible PLAYER-owned storage inside LootRadius.
//

static void CollectBetterLootingRadiusTargets(
    Character* centerCharacter,
    lektor<RootObject*>& characterTargets,
    lektor<RootObject*>& buildingTargets
);


static bool IsBetterLootingPlayerOwnedBuildingChain(
    Building* building,
    Faction* playerFaction
);


static Ogre::Vector3 GetBetterLootingBuildingWorldPosition(
    Building* building
);


static bool GetBetterLootingStoreStrictDistance(
    Building* building,
    Character* centerCharacter,
    float& distanceOut
)
{
    distanceOut = -1.0f;


    if (
        !building ||
        !centerCharacter ||
        !building->isValid() ||
        !centerCharacter->isPlayerCharacter()
    )
    {
        return false;
    }


    const float radiusInternal =
        g_betterLootingRules.lootRadius *
        10.0f;


    const Ogre::Vector3 centerPosition =
        centerCharacter->getPosition();


    TownBase* characterTown =
        centerCharacter->
            getCurrentTownLocation();


    TownBase* buildingTown =
        building->
            getCurrentTownLocation();


    // A storage in another town, or with an unresolved one-sided town,
    // is never a valid destination.
    if (buildingTown != characterTown)
    {
        return false;
    }


    const float objectDistance =
        (
            building->getPosition() -
            centerPosition
        ).length();


    if (
        objectDistance != objectDistance ||
        objectDistance < 0.0f ||
        objectDistance > radiusInternal
    )
    {
        return false;
    }


    // The physical AABB is an independent spatial signal. Requiring both
    // the object position and the loaded physical bounds to agree prevents
    // stale/local furniture coordinates from passing the deposit radius.
    const Ogre::Aabb& bounds =
        building->getAABB();


    const float halfSizeRadius =
        bounds.mHalfSize.length();


    const float boundsCenterDistance =
        (
            bounds.mCenter -
            centerPosition
        ).length();


    if (
        halfSizeRadius != halfSizeRadius ||
        boundsCenterDistance != boundsCenterDistance ||
        halfSizeRadius < 0.0f ||
        halfSizeRadius > 100000.0f ||
        boundsCenterDistance < 0.0f ||
        boundsCenterDistance >
            radiusInternal + halfSizeRadius
    )
    {
        return false;
    }


    // Validate the parent chain identity/town, but never use a parent
    // building center as the storage distance.
    Building* current =
        building;


    for (int depth = 0; depth < 8; ++depth)
    {
        Building* parent =
            current->
                furnitureParentBuilding();


        if (!parent)
        {
            distanceOut =
                objectDistance;

            return true;
        }


        if (
            parent == current ||
            !parent->isValid() ||
            parent->
                getCurrentTownLocation() !=
            characterTown
        )
        {
            return false;
        }


        current =
            parent;
    }


    // More than 8 parents or a cycle-like chain is not trusted.
    return false;
}


struct BetterLootingEmergencyStorageResult
{
    RootObject* object;
    Inventory* inventory;
    InventorySection* section;
    float distance;
    bool exact;
    bool ambiguousMutation;
};


static bool BetterLootingEmergencySectionHasExactItem(
    InventorySection* section,
    GameData* itemData
)
{
    if (
        !section ||
        !itemData
    )
    {
        return false;
    }


    const lektor<GameData*>& exactItems =
        section->getVeryLimitedSlot();


    for (
        uint32_t i = 0;
        i <
            static_cast<uint32_t>(
                exactItems.size()
            );
        ++i
    )
    {
        if (
            exactItems[i] ==
            itemData
        )
        {
            return true;
        }
    }


    return false;
}


static BetterLootingEmergencyStorageResult
TryBetterLootingNearestCompatiblePlayerStorage(
    Character* centerCharacter,
    Inventory* sourceInventory,
    Item* detachedItem,
    GameData* itemData,
    int sourceBefore,
    int detachedQuantity,
    bool sourceWasEquipped
)
{
    BetterLootingEmergencyStorageResult result = {
        nullptr,
        nullptr,
        nullptr,
        0.0f,
        false,
        false
    };


    if (
        !centerCharacter ||
        !centerCharacter->isPlayerCharacter() ||
        !sourceInventory ||
        !detachedItem ||
        !itemData ||
        detachedQuantity <= 0
    )
    {
        return result;
    }


    Faction* playerFaction =
        centerCharacter->getFaction();


    if (
        !playerFaction ||
        !playerFaction->isThePlayer()
    )
    {
        return result;
    }


    lektor<RootObject*> characterTargets;
    lektor<RootObject*> buildingTargets;


    CollectBetterLootingRadiusTargets(
        centerCharacter,
        characterTargets,
        buildingTargets
    );


    struct EmergencyCandidate
    {
        RootObject* object;
        Building* building;
        Inventory* inventory;
        float distance;
    };


    std::vector<EmergencyCandidate> candidates;


    for (
        uint32_t i = 0;
        i <
            static_cast<uint32_t>(
                buildingTargets.size()
            );
        ++i
    )
    {
        RootObject* object =
            buildingTargets[i];


        if (!object)
        {
            continue;
        }


        Building* building =
            static_cast<Building*>(
                object
            );


        if (
            !IsBetterLootingPlayerOwnedBuildingChain(
                building,
                playerFaction
            )
        )
        {
            continue;
        }


        const BuildingFunction function =
            building->
                getSpecialFunction();


        if (
            function != BF_GENERAL_STORAGE &&
            function != BF_RESOURCE_STORAGE
        )
        {
            continue;
        }


        Inventory* inventory =
            building->getInventory();


        if (
            !inventory ||
            inventory ==
                sourceInventory
        )
        {
            continue;
        }


        EmergencyCandidate candidate;

        candidate.object =
            object;

        candidate.building =
            building;

        candidate.inventory =
            inventory;

        if (
            !GetBetterLootingStoreStrictDistance(
                building,
                centerCharacter,
                candidate.distance
            )
        )
        {
            BlTrace(
                "BetterLooting: EMERGENCY_STORAGE REJECT strict radius or town"
            );

            continue;
        }


        candidates.push_back(
            candidate
        );
    }


    std::sort(
        candidates.begin(),
        candidates.end(),
        [](
            const EmergencyCandidate& left,
            const EmergencyCandidate& right
        ) -> bool
        {
            const BuildingFunction leftFunction =
                left.building->
                    getSpecialFunction();

            const BuildingFunction rightFunction =
                right.building->
                    getSpecialFunction();


            const int leftPriority =
                leftFunction ==
                    BF_GENERAL_STORAGE
                    ? 0
                    : 1;

            const int rightPriority =
                rightFunction ==
                    BF_GENERAL_STORAGE
                    ? 0
                    : 1;


            if (
                left.distance !=
                right.distance
            )
            {
                return
                    left.distance <
                    right.distance;
            }


            return
                leftPriority <
                rightPriority;
        }
    );


    for (
        std::size_t candidateIndex = 0;
        candidateIndex <
            candidates.size();
        ++candidateIndex
    )
    {
        EmergencyCandidate& candidate =
            candidates[candidateIndex];


        const BuildingFunction function =
            candidate.building->
                getSpecialFunction();


        const bool standardStorage =
            function ==
                BF_GENERAL_STORAGE ||
            function ==
                BF_RESOURCE_STORAGE;


        if (!standardStorage)
        {
            continue;
        }


        lektor<InventorySection*>& sections =
            candidate.inventory->
                getAllSections();


        for (
            uint32_t sectionIndex = 0;
            sectionIndex <
                static_cast<uint32_t>(
                    sections.size()
                );
            ++sectionIndex
        )
        {
            InventorySection* section =
                sections[sectionIndex];


            if (
                !section ||
                !section->getEnabled() ||
                section->containerSlot ||
                section->
                    isAnEquippedItemSection
            )
            {
                continue;
            }


            const bool exactSpecialized =
                BetterLootingEmergencySectionHasExactItem(
                    section,
                    itemData
                );


            const bool compatible =
                section->
                    isLimitedSlotCompatible(
                        detachedItem
                    ) ||
                (
                    function ==
                        BF_RESOURCE_STORAGE &&
                    section->
                        isLimitedSlotCompatible(
                            itemData
                        )
                );


            if (!compatible)
            {
                continue;
            }


            if (
                !section->
                    hasRoomForItem(
                        itemData,
                        detachedQuantity
                    )
            )
            {
                continue;
            }


            int x = 0;
            int y = 0;


            if (
                !section->
                    getValidInventoryPosition(
                        detachedItem,
                        x,
                        y
                    )
            )
            {
                continue;
            }


            const int destinationBefore =
                candidate.inventory->
                    countItems(
                        itemData
                    );


            const bool engineAdded =
                section->
                    addItem(
                        detachedItem,
                        1
                    );


            sourceInventory->
                notifyModified();

            candidate.inventory->
                notifyModified();


            const int sourceAfter =
                sourceInventory->
                    countItems(
                        itemData
                    );


            const int destinationAfter =
                candidate.inventory->
                    countItems(
                        itemData
                    );


            const int sourceDelta =
                sourceBefore -
                sourceAfter;


            const int destinationDelta =
                destinationAfter -
                destinationBefore;


            const bool sourceRemovalExact =
                sourceDelta ==
                    detachedQuantity ||
                (
                    sourceWasEquipped &&
                    detachedQuantity == 1 &&
                    sourceBefore == sourceAfter &&
                    !FindItemLocation(
                        sourceInventory,
                        detachedItem
                    ).found
                );


            const bool exactMove =
                sourceRemovalExact &&
                destinationDelta ==
                    detachedQuantity;


            char message[1024];

            sprintf_s(
                message,
                "BetterLooting: TRANSFER_EMERGENCY_STORAGE object=%p inventory=%p section=%p distanceMeters=%.2f function=%d exactSpecialized=%d engineReturn=%d exact=%d source=%d->%d destination=%d->%d quantity=%d",
                candidate.object,
                candidate.inventory,
                section,
                candidate.distance / 10.0f,
                static_cast<int>(
                    function
                ),
                exactSpecialized ? 1 : 0,
                engineAdded ? 1 : 0,
                exactMove ? 1 : 0,
                sourceBefore,
                sourceAfter,
                destinationBefore,
                destinationAfter,
                detachedQuantity
            );

            BlTrace(
                message
            );


            if (exactMove)
            {
                result.object =
                    candidate.object;

                result.inventory =
                    candidate.inventory;

                result.section =
                    section;

                result.distance =
                    candidate.distance;

                result.exact =
                    true;

                return result;
            }


            if (
                !sourceRemovalExact ||
                destinationDelta != 0
            )
            {
                result.ambiguousMutation =
                    true;

                return result;
            }
        }
    }


    return result;
}


static void LogBetterLootingTransferInvariantFault(
    std::size_t candidateIndex,
    Item* item,
    GameData* itemData,
    int movedQuantity,
    int sourceBefore,
    int sourceAfter,
    int destinationBefore,
    int destinationAfter,
    bool pointerInDestination,
    bool ownerPreserved,
    bool ownerOk
)
{
    char message[1024];

    sprintf_s(
        message,
        "BetterLooting: TRANSFER_INVARIANT_FAULT index=%u item=%p gameData=%p moved=%d source=%d->%d destination=%d->%d pointerInDestination=%d ownerPreserved=%d ownerOk=%d SESSION_TRANSFER_DISABLED=1",
        static_cast<unsigned int>(
            candidateIndex
        ),
        item,
        itemData,
        movedQuantity,
        sourceBefore,
        sourceAfter,
        destinationBefore,
        destinationAfter,
        pointerInDestination ? 1 : 0,
        ownerPreserved ? 1 : 0,
        ownerOk ? 1 : 0
    );

    BlTrace(message);


    g_betterLootingTransferFault = true;
}


static void ExecuteLootCandidate(
    Inventory* sourceInventory,
    InventoryGUI* destinationWindow,
    const BetterLootingCandidate& candidate,
    std::size_t candidateIndex
)
{
    if (g_betterLootingTransferFault)
    {
        BlTrace(
            "BetterLooting: TRANSFER_SKIP session transfer fault"
        );

        return;
    }


    if (
        !sourceInventory ||
        !destinationWindow ||
        !candidate.item
    )
    {
        BlTrace(
            "BetterLooting: TRANSFER_SKIP invalid route"
        );

        return;
    }


    GameData* itemData =
        candidate.item->getGameData();


    if (!itemData)
    {
        BlTrace(
            "BetterLooting: TRANSFER_SKIP missing item data"
        );

        return;
    }


    Character* stolenSourceCharacter =
        GetBetterLootingLootableCharacterSource(
            sourceInventory
        );


    const bool forceStolenFromCharacter =
        stolenSourceCharacter != NULL;


    if (
        forceStolenFromCharacter &&
        !g_betterLootingRules.allowStolen
    )
    {
        BlTrace(
            "BetterLooting: TRANSFER_SKIP body theft disabled by AllowStolen=0"
        );

        return;
    }


    Item* sourceItem =
        candidate.item;


    int movedTotal = 0;


    for (
        int transferPass = 0;
        transferPass < 1024;
        ++transferPass
    )
    {
        BetterLootingItemLocation liveLocation =
            FindItemLocation(
                sourceInventory,
                sourceItem
            );


        if (!liveLocation.found)
        {
            break;
        }


        const int sourceQuantity =
            GetBetterLootingItemQuantity(
                sourceItem
            );


        if (
            sourceQuantity <= 0 ||
            sourceQuantity > 100000
        )
        {
            char quantityMessage[256];

            sprintf_s(
                quantityMessage,
                "BetterLooting: TRANSFER_SKIP invalid quantity=%d",
                sourceQuantity
            );

            BlTrace(quantityMessage);

            return;
        }


        //
        // Transfer exactly one unit per transaction.
        //
        // Stack capacity is section-specific in Kenshi:
        // the same GameData can stack differently in a backpack,
        // the player main inventory, a general storage section,
        // or a dedicated storage section.
        //
        // Re-evaluating placement for quantity=1 on every pass lets the
        // engine fill the preferred backpack first and then fall back to
        // player main inventory without ever detaching more than the
        // selected destination can absorb.
        //

        BetterLootingDirectPlacement placement =
            FindBetterLootingDirectPlacement(
                destinationWindow,
                sourceItem,
                itemData,
                1
            );


        if (!placement.found)
        {
            char noRoomMessage[384];

            sprintf_s(
                noRoomMessage,
                "BetterLooting: TRANSFER index=%u result=SKIPPED reason=NO_ENGINE_SLOT remaining=%d moved=%d sourceUnchanged=1",
                static_cast<unsigned int>(
                    candidateIndex
                ),
                sourceQuantity,
                movedTotal
            );

            BlTrace(noRoomMessage);

            return;
        }


        const int moveQuantity = 1;

        int sourceMaxStack =
            sourceItem->
                isStackable(
                    liveLocation.section
                );

        if (sourceMaxStack <= 0)
        {
            sourceMaxStack = 1;
        }


        const bool sourceWasEquipped =
            liveLocation.section &&
            liveLocation.section->
                isAnEquippedItemSection;


        const int sourceBefore =
            sourceInventory->
                countItems(itemData);

        int destinationBefore =
            placement.inventory->
                countItems(itemData);


        const hand ownerBefore =
            sourceItem->getProperOwner();


        hand expectedOwner =
            ownerBefore;


        const bool sourceWillRemain =
            sourceQuantity >
            moveQuantity;


        Item* detachedItem =
            sourceInventory->
                removeItemDontDestroy_returnsItem(
                    sourceItem,
                    moveQuantity,
                    sourceWillRemain
                );


        if (!detachedItem)
        {
            BlTrace(
                "BetterLooting: TRANSFER_SKIP source remove failed"
            );

            return;
        }


        const int detachedQuantity =
            GetBetterLootingItemQuantity(
                detachedItem
            );


        if (
            detachedQuantity <= 0 ||
            detachedQuantity > 100000
        )
        {
            const bool detachedOwnerPreserved =
                detachedItem->
                    getProperOwner() ==
                    ownerBefore;

            const bool detachedOwnerOk =
                detachedOwnerPreserved ||
                (
                    ownerBefore.isNull() &&
                    !detachedItem->
                        isStolen(true)
                );


            LogBetterLootingTransferInvariantFault(
                candidateIndex,
                detachedItem,
                itemData,
                moveQuantity,
                sourceBefore,
                sourceInventory->
                    countItems(itemData),
                destinationBefore,
                placement.inventory->
                    countItems(itemData),
                false,
                detachedOwnerPreserved,
                detachedOwnerOk
            );

            return;
        }


        const bool detachedWasStolenBeforePolicy =
            detachedItem->
                isStolen(true);


        bool forcedStolenOwnerApplied =
            false;


        if (
            forceStolenFromCharacter &&
            !detachedWasStolenBeforePolicy
        )
        {
            // Use Kenshi's native Item theft ownership path.
            // Exact hand equality is not a valid success condition because
            // the engine can normalize the item's proper owner handle.
            detachedItem->
                notifyTheftFrom(
                    stolenSourceCharacter
                );


            forcedStolenOwnerApplied =
                detachedItem->
                    isStolen(true);


            expectedOwner =
                detachedItem->
                    getProperOwner();


            if (!forcedStolenOwnerApplied)
            {
                detachedItem->
                    setProperOwner(
                        ownerBefore
                    );


                const bool rollbackReported =
                    liveLocation.section->
                        addItem(
                            detachedItem,
                            1
                        );


                sourceInventory->
                    notifyModified();


                const BetterLootingItemLocation rollbackLocation =
                    FindItemLocation(
                        sourceInventory,
                        detachedItem
                    );


                if (
                    rollbackReported &&
                    rollbackLocation.found &&
                    rollbackLocation.section ==
                        liveLocation.section
                )
                {
                    BlTrace(
                        "BetterLooting: BODY_THEFT_NATIVE_MARK_FAIL rollback=OK"
                    );

                    return;
                }


                BlTrace(
                    "BetterLooting: BODY_THEFT_NATIVE_MARK_FAIL rollback=FAILED"
                );

                g_betterLootingTransferFault =
                    true;

                return;
            }
        }


        const bool detachedMustBeStolen =
            forceStolenFromCharacter ||
            detachedWasStolenBeforePolicy;


        int destinationStolenBefore =
            detachedMustBeStolen
                ? CountBetterLootingStolenQuantity(
                    placement.inventory,
                    itemData
                )
                : 0;


        //
        // detachedItem ya representa detachedQuantity unidades.
        // Para insertar ese stack separado, addItem recibe 1.
        // Se hace un solo intento sobre el destino elegido antes de separar.
        //

        bool engineAdded =
            placement.section->
                addItem(
                    detachedItem,
                    1
                );


        sourceInventory->
            notifyModified();

        placement.inventory->
            notifyModified();


        int sourceAfter =
            sourceInventory->
                countItems(itemData);

        int destinationAfter =
            placement.inventory->
                countItems(itemData);


        int sourceDelta =
            sourceBefore -
            sourceAfter;

        int destinationDelta =
            destinationAfter -
            destinationBefore;


        const bool bodySourceRemovalExact =
            forceStolenFromCharacter &&
            detachedQuantity == 1 &&
            !FindItemLocation(
                sourceInventory,
                detachedItem
            ).found;


        const bool sourceRemovalExact =
            sourceDelta ==
                detachedQuantity ||
            bodySourceRemovalExact ||
            (
                sourceWasEquipped &&
                detachedQuantity == 1 &&
                sourceBefore == sourceAfter &&
                !FindItemLocation(
                    sourceInventory,
                    detachedItem
                ).found
            );


        bool exactMove =
            sourceRemovalExact &&
            destinationDelta ==
                detachedQuantity;


        //
        // Symmetric runtime fallback:
        //
        // Regardless of preference, both PLAYER inventories are always
        // attempted. The checkbox changes only their order.
        //

        if (
            !exactMove &&
            destinationDelta == 0
        )
        {
            InventoryGUI* secondaryWindow =
                NULL;


            if (
                placement.window ==
                    g_playerBackpackWindow
            )
            {
                secondaryWindow =
                    g_playerMainWindow;
            }
            else if (
                placement.window ==
                    g_playerMainWindow
            )
            {
                secondaryWindow =
                    g_playerBackpackWindow;
            }


            if (
                secondaryWindow &&
                secondaryWindow !=
                    placement.window
            )
            {
                BetterLootingDirectPlacement secondaryPlacement =
                    FindBetterLootingDirectPlacementInWindow(
                        secondaryWindow,
                        detachedItem,
                        itemData,
                        1,
                        false
                    );


                if (secondaryPlacement.found)
                {
                    const int secondaryDestinationBefore =
                        secondaryPlacement.inventory->
                            countItems(itemData);


                    const int secondaryDestinationStolenBefore =
                        detachedMustBeStolen
                            ? CountBetterLootingStolenQuantity(
                                secondaryPlacement.inventory,
                                itemData
                            )
                            : 0;


                    const bool secondaryEngineAdded =
                        secondaryPlacement.section->
                            addItem(
                                detachedItem,
                                1
                            );


                    sourceInventory->
                        notifyModified();

                    secondaryPlacement.inventory->
                        notifyModified();


                    const int sourceAfterSecondaryAttempt =
                        sourceInventory->
                            countItems(itemData);


                    const int secondaryDestinationAfter =
                        secondaryPlacement.inventory->
                            countItems(itemData);


                    const int sourceDeltaSecondary =
                        sourceBefore -
                        sourceAfterSecondaryAttempt;


                    const int secondaryDestinationDelta =
                        secondaryDestinationAfter -
                        secondaryDestinationBefore;


                    const bool secondaryBodySourceRemovalExact =
                        forceStolenFromCharacter &&
                        detachedQuantity == 1 &&
                        !FindItemLocation(
                            sourceInventory,
                            detachedItem
                        ).found;


                    const bool secondarySourceRemovalExact =
                        sourceDeltaSecondary ==
                            detachedQuantity ||
                        secondaryBodySourceRemovalExact ||
                        (
                            sourceWasEquipped &&
                            detachedQuantity == 1 &&
                            sourceBefore ==
                                sourceAfterSecondaryAttempt &&
                            !FindItemLocation(
                                sourceInventory,
                                detachedItem
                            ).found
                        );


                    const bool secondaryExactMove =
                        secondarySourceRemovalExact &&
                        secondaryDestinationDelta ==
                            detachedQuantity;


                    char fallbackMessage[768];

                    sprintf_s(
                        fallbackMessage,
                        "BetterLooting: TRANSFER_RUNTIME_FALLBACK destination=%s engineReturn=%d exact=%d source=%d->%d destination=%d->%d quantity=%d",
                        LootDestinationName(
                            secondaryWindow
                        ),
                        secondaryEngineAdded ? 1 : 0,
                        secondaryExactMove ? 1 : 0,
                        sourceBefore,
                        sourceAfterSecondaryAttempt,
                        secondaryDestinationBefore,
                        secondaryDestinationAfter,
                        detachedQuantity
                    );

                    BlTrace(
                        fallbackMessage
                    );


                    if (secondaryExactMove)
                    {
                        placement =
                            secondaryPlacement;

                        destinationBefore =
                            secondaryDestinationBefore;

                        destinationStolenBefore =
                            secondaryDestinationStolenBefore;

                        sourceAfter =
                            sourceAfterSecondaryAttempt;

                        destinationAfter =
                            secondaryDestinationAfter;

                        sourceDelta =
                            sourceDeltaSecondary;

                        destinationDelta =
                            secondaryDestinationDelta;

                        engineAdded =
                            secondaryEngineAdded;

                        exactMove =
                            true;
                    }
                    else if (
                        secondaryDestinationDelta != 0 ||
                        !secondarySourceRemovalExact
                    )
                    {
                        const bool failedOwnerOk =
                            ownerBefore.isNull() &&
                            !detachedMustBeStolen;


                        LogBetterLootingTransferInvariantFault(
                            candidateIndex,
                            detachedItem,
                            itemData,
                            detachedQuantity,
                            sourceBefore,
                            sourceAfterSecondaryAttempt,
                            secondaryDestinationBefore,
                            secondaryDestinationAfter,
                            false,
                            false,
                            failedOwnerOk
                        );

                        return;
                    }
                }
            }
        }


        if (!exactMove)
        {
            //
            // Safe rollback is possible only when destination did not change.
            //
            // Return the single detached unit to the exact source section
            // that released it. That section has one unit of newly freed
            // capacity, so its own item/section stack rules are respected.
            //

            if (destinationDelta == 0)
            {
                if (forcedStolenOwnerApplied)
                {
                    detachedItem->
                        setProperOwner(
                            ownerBefore
                        );
                }


                const bool rollbackReported =
                    liveLocation.section->
                        addItem(
                            detachedItem,
                            1
                        );


                sourceInventory->
                    notifyModified();

                placement.inventory->
                    notifyModified();


                const int sourceAfterRollback =
                    sourceInventory->
                        countItems(itemData);

                const int destinationAfterRollback =
                    placement.inventory->
                        countItems(itemData);

                const BetterLootingItemLocation
                    rollbackLocation =
                        FindItemLocation(
                            sourceInventory,
                            detachedItem
                        );


                const bool rollbackSourceExact =
                    sourceWasEquipped
                        ? (
                            rollbackReported &&
                            rollbackLocation.found &&
                            rollbackLocation.section ==
                                liveLocation.section
                        )
                        : (
                            sourceAfterRollback ==
                                sourceBefore
                        );


                const bool rollbackExact =
                    rollbackSourceExact &&
                    destinationAfterRollback ==
                        destinationBefore;


                char rollbackMessage[768];

                sprintf_s(
                    rollbackMessage,
                    "BetterLooting: TRANSFER_ROLLBACK engineReturn=%d rollbackReturn=%d exact=%d source=%d->%d destination=%d->%d quantity=%d sourceMaxStack=%d destinationMaxStack=%d",
                    engineAdded ? 1 : 0,
                    rollbackReported ? 1 : 0,
                    rollbackExact ? 1 : 0,
                    sourceBefore,
                    sourceAfterRollback,
                    destinationBefore,
                    destinationAfterRollback,
                    detachedQuantity,
                    sourceMaxStack,
                    placement.maxStack
                );

                BlTrace(
                    rollbackMessage
                );


                if (rollbackExact)
                {
                    return;
                }


                sourceAfter =
                    sourceAfterRollback;

                destinationAfter =
                    destinationAfterRollback;

                sourceDelta =
                    sourceBefore -
                    sourceAfter;

                destinationDelta =
                    destinationAfter -
                    destinationBefore;


                //
                // Emergency step 4:
                //
                // Only when the exact source rollback failed without
                // changing source or destination state, the detached unit is
                // still available for one final safe route.
                //

                const bool sourceStillDetached =
                    sourceDelta ==
                        detachedQuantity ||
                    (
                        forceStolenFromCharacter &&
                        detachedQuantity == 1 &&
                        !FindItemLocation(
                            sourceInventory,
                            detachedItem
                        ).found
                    ) ||
                    (
                        sourceWasEquipped &&
                        sourceBefore == sourceAfter &&
                        !FindItemLocation(
                            sourceInventory,
                            detachedItem
                        ).found
                    );


                if (
                    sourceStillDetached &&
                    destinationDelta == 0
                )
                {
                    if (
                        forceStolenFromCharacter &&
                        !detachedItem->
                            isStolen(true)
                    )
                    {
                        detachedItem->
                            notifyTheftFrom(
                                stolenSourceCharacter
                            );


                        expectedOwner =
                            detachedItem->
                                getProperOwner();
                    }


                    Character* centerCharacter =
                        destinationWindow
                            ? destinationWindow->
                                getCallbackCharacter()
                            : NULL;


                    if (
                        !centerCharacter &&
                        g_playerMainWindow
                    )
                    {
                        centerCharacter =
                            g_playerMainWindow->
                                getCallbackCharacter();
                    }


                    BetterLootingEmergencyStorageResult emergency =
                        TryBetterLootingNearestCompatiblePlayerStorage(
                            centerCharacter,
                            sourceInventory,
                            detachedItem,
                            itemData,
                            sourceBefore,
                            detachedQuantity,
                            sourceWasEquipped
                        );


                    if (emergency.exact)
                    {
                        BlTrace(
                            "BetterLooting: TRANSFER_EMERGENCY_STORAGE result=OK"
                        );

                        return;
                    }


                    if (emergency.ambiguousMutation)
                    {
                        const bool failedOwnerOk =
                            ownerBefore.isNull() &&
                            !detachedMustBeStolen;


                        LogBetterLootingTransferInvariantFault(
                            candidateIndex,
                            detachedItem,
                            itemData,
                            detachedQuantity,
                            sourceBefore,
                            sourceInventory->
                                countItems(
                                    itemData
                                ),
                            destinationBefore,
                            placement.inventory->
                                countItems(
                                    itemData
                                ),
                            false,
                            false,
                            failedOwnerOk
                        );

                        return;
                    }
                }
            }


            const bool failedOwnerPreserved =
                false;

            const bool failedOwnerOk =
                ownerBefore.isNull() &&
                !detachedMustBeStolen;


            LogBetterLootingTransferInvariantFault(
                candidateIndex,
                detachedItem,
                itemData,
                detachedQuantity,
                sourceBefore,
                sourceAfter,
                destinationBefore,
                destinationAfter,
                false,
                failedOwnerPreserved,
                failedOwnerOk
            );

            return;
        }


        const bool pointerInDestination =
            placement.maxStack <= 1 &&
            placement.inventory->
                hasItem(detachedItem);


        // Corpse/KO transfers can re-home or merge the detached item pointer.
        // For that route, exact source removal + destination count delta and
        // stolen-quantity validation are the authoritative invariants.
        const bool pointerOk =
            forceStolenFromCharacter
                ? exactMove
                : (
                    placement.maxStack > 1 ||
                    pointerInDestination
                );


        const bool ownerPreserved =
            !forceStolenFromCharacter &&
            pointerInDestination &&
            detachedItem->
                getProperOwner() ==
                expectedOwner;


        const int destinationStolenAfter =
            detachedMustBeStolen
                ? CountBetterLootingStolenQuantity(
                    placement.inventory,
                    itemData
                )
                : destinationStolenBefore;


        const int stolenQuantityDelta =
            destinationStolenAfter -
            destinationStolenBefore;


        const bool stolenStatePreserved =
            !detachedMustBeStolen ||
            stolenQuantityDelta >=
                detachedQuantity;


        const bool ownerOk =
            detachedMustBeStolen
                ? stolenStatePreserved
                : (
                    expectedOwner.isNull() ||
                    ownerPreserved
                );


        if (
            !pointerOk ||
            !ownerOk
        )
        {
            LogBetterLootingTransferInvariantFault(
                candidateIndex,
                detachedItem,
                itemData,
                detachedQuantity,
                sourceBefore,
                sourceAfter,
                destinationBefore,
                destinationAfter,
                pointerInDestination,
                ownerPreserved,
                ownerOk
            );

            return;
        }


        if (!engineAdded)
        {
            BlTrace(
                "BetterLooting: TRANSFER_ENGINE_FALSE_BUT_EXACT_MOVE accepted=1"
            );
        }


        RegisterBetterLootingProtectedLoot(
            placement.inventory,
            itemData,
            detachedQuantity
        );


        RegisterBetterLootingLootOrigin(
            placement.inventory,
            itemData,
            sourceInventory,
            detachedQuantity
        );


        movedTotal +=
            detachedQuantity;


        char successMessage[768];

        sprintf_s(
            successMessage,
            "BetterLooting: TRANSFER_DIRECT index=%u pass=%d result=OK destination=%s quantity=%d movedTotal=%d source=%d->%d destinationCount=%d->%d sourceMaxStack=%d destinationMaxStack=%d equipment=%d bodyTheft=%d forcedOwner=%d stolenRequired=%d stolenDelta=%d ownerPreserved=%d stolenStatePreserved=%d ownerOk=%d",
            static_cast<unsigned int>(
                candidateIndex
            ),
            transferPass,
            LootDestinationName(
                placement.window
            ),
            detachedQuantity,
            movedTotal,
            sourceBefore,
            sourceAfter,
            destinationBefore,
            destinationAfter,
            sourceMaxStack,
            placement.maxStack,
            placement.equipmentSection ? 1 : 0,
            forceStolenFromCharacter ? 1 : 0,
            forcedStolenOwnerApplied ? 1 : 0,
            detachedMustBeStolen ? 1 : 0,
            stolenQuantityDelta,
            ownerPreserved ? 1 : 0,
            stolenStatePreserved ? 1 : 0,
            ownerOk ? 1 : 0
        );

        BlTrace(successMessage);


        if (!sourceWillRemain)
        {
            return;
        }


        if (
            !sourceInventory->
                hasItem(sourceItem)
        )
        {
            const bool sourceOwnerPreserved =
                sourceItem->
                    getProperOwner() ==
                    ownerBefore;

            const bool sourceOwnerOk =
                sourceOwnerPreserved ||
                (
                    ownerBefore.isNull() &&
                    !sourceItem->
                        isStolen(true)
                );


            LogBetterLootingTransferInvariantFault(
                candidateIndex,
                sourceItem,
                itemData,
                movedTotal,
                sourceBefore,
                sourceInventory->
                    countItems(itemData),
                destinationBefore,
                placement.inventory->
                    countItems(itemData),
                false,
                sourceOwnerPreserved,
                sourceOwnerOk
            );

            return;
        }
    }


    BlTrace(
        "BetterLooting: TRANSFER_SKIP safety pass limit"
    );

    g_betterLootingTransferFault = true;
}


static int GetBetterLootingItemFunctionValue(
    Item* item
)
{
    if (!item)
    {
        return static_cast<int>(
            ITEM_NO_FUNCTION
        );
    }

    const unsigned char* base =
        reinterpret_cast<const unsigned char*>(
            item
        );

    const ItemFunction functionValue =
        *reinterpret_cast<const ItemFunction*>(
            base + 0x124
        );

    return static_cast<int>(
        functionValue
    );
}


static int GetBetterLootingEffectiveItemFunctionValue(
    Item* item,
    int rawFunctionValue
)
{
    if (
        item &&
        item->isResearchArtifact()
    )
    {
        return static_cast<int>(
            ITEM_BOOK
        );
    }


    return rawFunctionValue;
}

static void EvaluateTargetInventory(
    InventoryGUI* sourceWindow,
    Inventory* inventory,
    InventoryGUI* destinationWindow
)
{
    if (
        !inventory ||
        g_betterLootingTransferFault
    )
    {
        return;
    }

    const lektor<InventorySection*>& sections =
        inventory->getAllSections();

    ogre_unordered_set<GameData*>::type BetterLootingResourceItems;

    inventory->getResourceItems(
        BetterLootingResourceItems,
        true
    );


    const std::vector<BetterLootingCandidate> candidates =
        EnumerateInventoryCandidates(inventory);


    Character* stolenSourceCharacter =
        GetBetterLootingLootableCharacterSource(
            inventory
        );


    const bool bodyTheftSource =
        stolenSourceCharacter != NULL;


    char header[384];

    sprintf_s(
        header,
        "BetterLooting: EVALUATE inventory=%p sections=%u candidates=%u bodyTheftSource=%d allowStolen=%d",
        inventory,
        static_cast<unsigned int>(sections.size()),
        static_cast<unsigned int>(candidates.size()),
        bodyTheftSource ? 1 : 0,
        g_betterLootingRules.allowStolen ? 1 : 0
    );

    BlTrace(header);

    for (
        std::size_t candidateIndex = 0;
        candidateIndex < candidates.size();
        ++candidateIndex
    )
    {
        const BetterLootingCandidate& candidate =
            candidates[candidateIndex];

        Item* item = candidate.item;

        if (!item)
            continue;

        const int valueSingle =
            item->getValueSingle(true);

        const int valueAll =
            item->getValueAll(true);

        const float weightSingle =
            item->getItemWeightSingle();

        const float weightAll =
            item->getItemWeight();

        const float valuePerKg =
            weightSingle > 0.001f
                ? static_cast<float>(valueSingle) / weightSingle
                : 0.0f;

        const int type =
            static_cast<int>(item->getItemType());

        const bool stolenBefore =
            item->isStolen(true);


        const bool effectiveStolen =
            stolenBefore ||
            bodyTheftSource;


        const int itemFunctionValue =
            GetBetterLootingItemFunctionValue(
                item
            );

        const bool isResearchArtifact =
            item->isResearchArtifact();

        const int effectiveItemFunctionValue =
            GetBetterLootingEffectiveItemFunctionValue(
                item,
                itemFunctionValue
            );


        GameData* itemGameData =
            item->getGameData();


        const bool isResource =
            itemGameData &&
            BetterLootingResourceItems.find(
                itemGameData
            ) != BetterLootingResourceItems.end();

        const BetterLootingDecision decision =
            EvaluateLootDecision(
                valueSingle,
                weightSingle,
                effectiveStolen,
                type,
                effectiveItemFunctionValue,
                isResource
            );

        char message[768];

        sprintf_s(
            message,
            "BetterLooting: CANDIDATE index=%u item=%p type=%d valueSingle=%d valueAll=%d weightSingle=%.3f weightAll=%.3f valuePerKg=%.2f stolenBefore=%d effectiveStolen=%d bodyTheftSource=%d section=%p x=%d y=%d",
            static_cast<unsigned int>(candidateIndex),
            item,
            type,
            valueSingle,
            valueAll,
            weightSingle,
            weightAll,
            valuePerKg,
            stolenBefore ? 1 : 0,
            effectiveStolen ? 1 : 0,
            bodyTheftSource ? 1 : 0,
            candidate.section,
            candidate.x,
            candidate.y
        );

        BlTrace(message);

        char planMessage[256];

        sprintf_s(
            planMessage,
            "BetterLooting: PLAN index=%u decision=%s",
            static_cast<unsigned int>(candidateIndex),
            BetterLootingDecisionName(decision)
        );

        BlTrace(planMessage);


        const hand& properOwner =
            item->getProperOwner();

        char itemAudit[1024];

        sprintf_s(
            itemAudit,
            "BetterLooting: AUDIT_ITEM index=%u inventory=%p item=%p gameData=%p itemType=%d rawFunction=%d effectiveFunction=%d researchArtifact=%d isResource=%d stolen=%d properOwnerNull=%d properOwnerValid=%d properOwnerType=%d properOwnerContainer=%u properOwnerContainerSerial=%u properOwnerIndex=%u properOwnerSerial=%u valueSingle=%d weightSingle=%.3f valuePerKg=%.2f decision=%s",
            static_cast<unsigned int>(candidateIndex),
            inventory,
            item,
            itemGameData,
            type,
            itemFunctionValue,
            effectiveItemFunctionValue,
            isResearchArtifact ? 1 : 0,
            isResource ? 1 : 0,
            effectiveStolen ? 1 : 0,
            properOwner.isNull() ? 1 : 0,
            properOwner.isValid() ? 1 : 0,
            static_cast<int>(properOwner.type),
            properOwner.container,
            properOwner.containerSerial,
            properOwner.index,
            properOwner.serial,
            valueSingle,
            weightSingle,
            valuePerKg,
            BetterLootingDecisionName(decision)
        );

        BlTrace(itemAudit);


        if (decision == BetterLootingDecision::LOOT)
        {
            ExecuteLootCandidate(
                inventory,
                destinationWindow,
                candidate,
                static_cast<unsigned int>(candidateIndex)
            );


            if (g_betterLootingTransferFault)
            {
                BlTrace(
                    "BetterLooting: EVALUATE ABORT transfer invariant fault"
                );

                break;
            }
        }
    }
}

static std::string BetterLootingIniPath();

static bool LoadBetterLootingRules()
{
    char exePath[MAX_PATH] = {};

    const DWORD length =
        GetModuleFileNameA(
            NULL,
            exePath,
            MAX_PATH
        );

    if (
        length == 0 ||
        length >= MAX_PATH
    )
    {
        BlTrace(
            "BetterLooting: CONFIG_ERROR executable path"
        );

        return false;
    }


    std::string configPath(
        exePath,
        length
    );

    const std::size_t slash =
        configPath.find_last_of("\\/");

    if (slash == std::string::npos)
    {
        BlTrace(
            "BetterLooting: CONFIG_ERROR base path"
        );

        return false;
    }


    configPath.resize(slash + 1);

    configPath +=
        "..\\mods\\BetterLooting\\BetterLooting.ini";

    // Тот же файл, что пишут панель и MCM: путь от самой DLL, а не от
    // исполняемого файла (exe - загрузчик RE_Kenshi, совпадало случайно).
    configPath = BetterLootingIniPath();

    g_betterLootingDebugLog =
        GetPrivateProfileIntA(
            "Debug",
            "Log",
            0,
            configPath.c_str()
        ) != 0;


    g_betterLootingRules.minValue =
        GetPrivateProfileIntA(
            "Rules",
            "MinValue",
            500,
            configPath.c_str()
        );


    char ratioBuffer[64] = {};

    GetPrivateProfileStringA(
        "Rules",
        "MinValuePerKg",
        "300.0",
        ratioBuffer,
        static_cast<DWORD>(
            sizeof(ratioBuffer)
        ),
        configPath.c_str()
    );


    g_betterLootingRules.minValuePerKg =
        static_cast<float>(std::strtod(
            ratioBuffer,
            NULL
        ));


    g_betterLootingRules.allowStolen =
        GetPrivateProfileIntA(
            "Rules",
            "AllowStolen",
            0,
            configPath.c_str()
        ) != 0;


        g_betterLootingRules.lootWeapons =
        GetPrivateProfileIntA(
            "Categories", "Weapons", 1,
            configPath.c_str()
        ) != 0;

    g_betterLootingRules.lootArmour =
        GetPrivateProfileIntA(
            "Categories", "Armour", 1,
            configPath.c_str()
        ) != 0;

    g_betterLootingRules.lootFood =
        GetPrivateProfileIntA(
            "Categories", "Food", 1,
            configPath.c_str()
        ) != 0;

    g_betterLootingRules.lootMedicine =
        GetPrivateProfileIntA(
            "Categories", "Medicine", 1,
            configPath.c_str()
        ) != 0;

    g_betterLootingRules.lootTools =
        GetPrivateProfileIntA(
            "Categories", "Tools", 1,
            configPath.c_str()
        ) != 0;

    g_betterLootingRules.lootMaterials =
        GetPrivateProfileIntA(
            "Categories", "Materials", 1,
            configPath.c_str()
        ) != 0;

    g_betterLootingRules.lootBlueprints =
        GetPrivateProfileIntA(
            "Categories", "Blueprints", 1,
            configPath.c_str()
        ) != 0;

    g_betterLootingRules.lootAmmo =
        GetPrivateProfileIntA(
            "Categories", "Ammo", 1,
            configPath.c_str()
        ) != 0;

    g_betterLootingRules.lootRobotics =
        GetPrivateProfileIntA(
            "Categories", "Robotics", 1,
            configPath.c_str()
        ) != 0;

    g_betterLootingRules.lootBooks =
        GetPrivateProfileIntA(
            "Categories", "Books", 1,
            configPath.c_str()
        ) != 0;

    g_betterLootingRules.lootNarcotics =
        GetPrivateProfileIntA(
            "Categories", "Narcotics", 0,
            configPath.c_str()
        ) != 0;

    g_betterLootingRules.lootSeveredLimbs =
        GetPrivateProfileIntA(
            "Categories", "SeveredLimbs", 0,
            configPath.c_str()
        ) != 0;

    g_betterLootingRules.lootOther =
        GetPrivateProfileIntA(
            "Categories", "Other", 1,
            configPath.c_str()
        ) != 0;

    g_betterLootingRules.bestItemsFirst =
        GetPrivateProfileIntA(
            "Behavior",
            "BestItemsFirst",
            1,
            configPath.c_str()
        ) != 0;

    g_betterLootingRules.preferBackpack =
        GetPrivateProfileIntA(
            "Behavior",
            "PreferBackpack",
            1,
            configPath.c_str()
        ) != 0;

    g_betterLootingRules.activationVirtualKey =
        GetPrivateProfileIntA(
            "Behavior",
            "ActivationVirtualKey",
            45,
            configPath.c_str()
        );

    if (
        g_betterLootingRules.activationVirtualKey < 1 ||
        g_betterLootingRules.activationVirtualKey > 255
    )
    {
        g_betterLootingRules.activationVirtualKey = 45;
    }

    char lootRadiusBuffer[64] = { 0 };

    GetPrivateProfileStringA(
        "General",
        "LootRadius",
        "20.0",
        lootRadiusBuffer,
        64,
        configPath.c_str()
    );


    const float BETTER_LOOTING_MIN_RADIUS_METERS =
        1.0f;

    const float BETTER_LOOTING_MAX_RADIUS_METERS =
        100.0f;

    const float BETTER_LOOTING_DEFAULT_RADIUS_METERS =
        20.0f;


    float parsedLootRadius =
        BETTER_LOOTING_DEFAULT_RADIUS_METERS;


    if (
        sscanf_s(
            lootRadiusBuffer,
            "%f",
            &parsedLootRadius
        ) != 1
    )
    {
        parsedLootRadius =
            BETTER_LOOTING_DEFAULT_RADIUS_METERS;
    }


    if (
        parsedLootRadius <
        BETTER_LOOTING_MIN_RADIUS_METERS
    )
    {
        parsedLootRadius =
            BETTER_LOOTING_MIN_RADIUS_METERS;
    }


    if (
        parsedLootRadius >
        BETTER_LOOTING_MAX_RADIUS_METERS
    )
    {
        parsedLootRadius =
            BETTER_LOOTING_MAX_RADIUS_METERS;
    }


    g_betterLootingRules.lootRadius =
        parsedLootRadius;


    {
        char radiusMessage[256];

        sprintf_s(
            radiusMessage,
            "BetterLooting: RADIUS_CONFIG meters=%.2f internal=%.2f min=%.2f max=%.2f",
            g_betterLootingRules.lootRadius,
            g_betterLootingRules.lootRadius * 10.0f,
            BETTER_LOOTING_MIN_RADIUS_METERS,
            BETTER_LOOTING_MAX_RADIUS_METERS
        );

        BlTrace(radiusMessage);
    }

if (g_betterLootingRules.minValue < 0)
    {
        g_betterLootingRules.minValue = 0;
    }


    if (
        g_betterLootingRules.minValuePerKg <
        0.0f
    )
    {
        g_betterLootingRules.minValuePerKg =
            0.0f;
    }


    char message[512];

    sprintf_s(
        message,
        "BetterLooting: CONFIG minValue=%d minValuePerKg=%.2f allowStolen=%d path=%s",
        g_betterLootingRules.minValue,
        g_betterLootingRules.minValuePerKg,
        g_betterLootingRules.allowStolen ? 1 : 0,
        configPath.c_str()
    );

    BlTrace(message);

    return true;
}




static bool ShouldRunSmartLoot()
{
    return (
        GetAsyncKeyState(
            g_betterLootingRules.activationVirtualKey
        ) & 0x8000
    ) != 0;
}

typedef void (*InventoryGUIUpdateFn)(
    InventoryGUI*
);


static InventoryGUIUpdateFn
    g_origInventoryGUIUpdate = NULL;


static InventoryGUI* g_pendingLootSource = NULL;
static Inventory* g_pendingLootInventory = NULL;
static InventoryGUI* g_pendingLootDestination = NULL;

static InventoryGUI* g_lastLootTargetWindow = NULL;
static Inventory* g_lastLootTargetInventory = NULL;

static int g_pendingLootFrames = 0;

static bool g_pendingLootRunning = false;

static bool g_radialAfterDirectPending = false;
static Inventory* g_radialAfterDirectExcludedInventory = NULL;


static void QueueSmartLoot(
    InventoryGUI* sourceWindow,
    InventoryGUI* destinationWindow
)
{
    if (
        !sourceWindow ||
        !destinationWindow ||
        sourceWindow == destinationWindow
    )
    {
        BlTrace(
            "BetterLooting: SMART_LOOT QUEUE_SKIP invalid route"
        );

        return;
    }


    Inventory* sourceInventory =
        sourceWindow == g_lastLootTargetWindow
            ? g_lastLootTargetInventory
            : NULL;

    if (!sourceInventory)
    {
        BlTrace(
            "BetterLooting: SMART_LOOT QUEUE_SKIP invalid source inventory"
        );

        return;
    }


    if (
        g_pendingLootRunning ||
        g_pendingLootSource ||
        g_pendingLootInventory
    )
    {
        BlTrace(
            "BetterLooting: SMART_LOOT QUEUE_SKIP busy"
        );

        return;
    }


    g_pendingLootSource =
        sourceWindow;

    g_pendingLootInventory =
        sourceInventory;

    g_pendingLootDestination =
        destinationWindow;

    g_pendingLootFrames = 1;


    g_radialAfterDirectPending = true;

    g_radialAfterDirectExcludedInventory =
        sourceInventory;


    BlTrace(
        "BetterLooting: SMART_LOOT QUEUED"
    );

    BlTrace(
        "BetterLooting: SMART_LOOT DIRECT_THEN_RADIAL_QUEUED"
    );
}


static Character* GetBetterLootingRadiusCenter(
    InventoryGUI* livePlayerWindow
)
{
    //
    // Resolve the radial center from the InventoryGUI currently executing
    // this hook. Unlike cached GUI pointers or PlayerInterface helper
    // results, this object is live for the duration of the current update.
    //

    if (!livePlayerWindow)
    {
        BlTrace(
            "BetterLooting: RADIAL_CENTER SKIP no live player window"
        );

        return NULL;
    }


    Character* character =
        livePlayerWindow->getCallbackCharacter();


    if (!character)
    {
        BlTrace(
            "BetterLooting: RADIAL_CENTER SKIP no live callback character"
        );

        return NULL;
    }


    if (!character->isPlayerCharacter())
    {
        BlTrace(
            "BetterLooting: RADIAL_CENTER SKIP callback is not player"
        );

        return NULL;
    }


    char message[192];

    sprintf_s(
        message,
        "BetterLooting: RADIAL_CENTER liveGui=%p character=%p",
        livePlayerWindow,
        character
    );

    BlTrace(message);


    return character;
}


static Ogre::Vector3 GetBetterLootingBuildingWorldPosition(
    Building* building
)
{
    if (!building)
    {
        return Ogre::Vector3::ZERO;
    }


    // Interior furniture can expose coordinates relative to its layout/building.
    // For radius safety, anchor it to the top-level parent building, whose
    // position is in the same world space as the player character.
    Building* worldAnchor =
        building;


    for (int depth = 0; depth < 8; ++depth)
    {
        Building* parent =
            worldAnchor->
                furnitureParentBuilding();


        if (
            !parent ||
            parent == worldAnchor
        )
        {
            break;
        }


        worldAnchor =
            parent;
    }


    return
        worldAnchor->getPosition();
}

static Ogre::Vector3 GetBetterLootingObjectWorldPosition(
    RootObject* object,
    itemType type
)
{
    if (!object)
    {
        return Ogre::Vector3::ZERO;
    }


    if (type == BUILDING)
    {
        return
            GetBetterLootingBuildingWorldPosition(
                static_cast<Building*>(
                    object
                )
            );
    }


    return
        object->getPosition();
}


static float GetBetterLootingWorldDistanceToCharacter(
    RootObject* object,
    itemType type,
    Character* centerCharacter
)
{
    if (
        !object ||
        !centerCharacter
    )
    {
        return -1.0f;
    }


    const Ogre::Vector3 worldPosition =
        GetBetterLootingObjectWorldPosition(
            object,
            type
        );


    return
        (
            worldPosition -
            centerCharacter->getPosition()
        ).length();
}


static bool IsBetterLootingInsideConfiguredRadius(
    RootObject* object,
    itemType type,
    Character* centerCharacter
)
{
    const float distance =
        GetBetterLootingWorldDistanceToCharacter(
            object,
            type,
            centerCharacter
        );


    const float radiusInternal =
        g_betterLootingRules.lootRadius *
        10.0f;


    return
        distance >= 0.0f &&
        distance <= radiusInternal;
}


static void CollectBetterLootingRadiusTargets(
    Character* centerCharacter,
    lektor<RootObject*>& characterTargets,
    lektor<RootObject*>& buildingTargets
)
{
    characterTargets.clear();
    buildingTargets.clear();


    if (
        !ou ||
        !centerCharacter
    )
    {
        return;
    }

    const Ogre::Vector3 centerPosition =
        centerCharacter->getPosition();


    const float radius =
        g_betterLootingRules.lootRadius *
        10.0f;

    ou->getObjectsWithinSphere(
        characterTargets,
        centerPosition,
        radius,
        CHARACTER,
        256,
        centerCharacter
    );

    ou->getObjectsWithinSphere(
        buildingTargets,
        centerPosition,
        radius,
        BUILDING,
        256,
        centerCharacter
    );
}

struct BetterLootingRadiusTarget
{
    RootObject* object;
    itemType type;
};


static bool IsBetterLootingCharacterTarget(
    Character* character
)
{
    if (!character)
    {
        return false;
    }


    //
    // Nunca saquear personajes del jugador.
    //

    if (character->isPlayerCharacter())
    {
        return false;
    }


    //
    // Solo muertos o inconscientes.
    //

    if (
        !character->isDead() &&
        !character->isUnconcious()
    )
    {
        return false;
    }


    Inventory* inventory =
        character->getInventory();


    if (
        !inventory ||
        inventory->getNumItems() <= 0
    )
    {
        return false;
    }


    return true;
}


static void LogBetterLootingBuildingDiagnostic(
    RootObject* object,
    Character* centerCharacter,
    unsigned int scanIndex
)
{
    if (!object)
    {
        char nullMessage[192];

        sprintf_s(
            nullMessage,
            "BetterLooting: AUDIT_BUILDING scan=%u object=NULL",
            scanIndex
        );

        BlTrace(nullMessage);

        return;
    }


    Building* building =
        static_cast<Building*>(
            object
        );


    const bool valid =
        building->isValid();


    if (!valid)
    {
        char invalidMessage[256];

        sprintf_s(
            invalidMessage,
            "BetterLooting: AUDIT_BUILDING scan=%u object=%p valid=0",
            scanIndex,
            building
        );

        BlTrace(invalidMessage);

        return;
    }


    const BuildingClassType classType =
        building->getBuildingClass();

    const BuildingFunction function =
        building->getSpecialFunction();

    const bool playerBuilding =
        building->isThePlayer();

    Faction* faction =
        building->getFaction();

    const bool playerFaction =
        faction &&
        faction->isThePlayer();

    const hand& buildingHandle =
        building->getHandle();

    Ownerships* ownerships =
        building->getOwnerships();

    Inventory* inventory =
        building->getInventory();

    const int itemCount =
        inventory
            ? inventory->getNumItems()
            : -1;


    float distance = -1.0f;


    if (centerCharacter)
    {
        distance =
            GetBetterLootingWorldDistanceToCharacter(
                building,
                BUILDING,
                centerCharacter
            );
    }


    const bool acceptedByCurrentFilter =
        playerBuilding &&
        playerFaction &&
        (
            function == BF_GENERAL_STORAGE ||
            function == BF_RESOURCE_STORAGE
        ) &&
        inventory &&
        itemCount > 0;


    char message[1024];

    sprintf_s(
        message,
        "BetterLooting: AUDIT_BUILDING scan=%u object=%p valid=1 distance=%.2f class=%d function=%d isThePlayer=%d faction=%p factionIsPlayer=%d handleNull=%d handleValid=%d handleType=%d handleContainer=%u handleContainerSerial=%u handleIndex=%u handleSerial=%u ownerships=%p inventory=%p items=%d currentFilterAccept=%d",
        scanIndex,
        building,
        distance,
        static_cast<int>(classType),
        static_cast<int>(function),
        playerBuilding ? 1 : 0,
        faction,
        playerFaction ? 1 : 0,
        buildingHandle.isNull() ? 1 : 0,
        buildingHandle.isValid() ? 1 : 0,
        static_cast<int>(buildingHandle.type),
        buildingHandle.container,
        buildingHandle.containerSerial,
        buildingHandle.index,
        buildingHandle.serial,
        ownerships,
        inventory,
        itemCount,
        acceptedByCurrentFilter ? 1 : 0
    );

    BlTrace(message);
}


static bool IsBetterLootingBuildingTarget(
    RootObject* object
)
{
    if (!object)
    {
        return false;
    }


    Building* building =
        static_cast<Building*>(
            object
        );


    if (!building->isValid())
    {
        return false;
    }


    //
    // Ownership demostrado por runtime:
    // solo edificios que Kenshi marca como del jugador.
    //

    if (!building->isThePlayer())
    {
        return false;
    }


    Faction* faction =
        building->getFaction();


    if (
        !faction ||
        !faction->isThePlayer()
    )
    {
        return false;
    }


    const BuildingFunction function =
        building->getSpecialFunction();


    //
    // Los cofres reales del jugador observados en runtime
    // aparecen como BF_GENERAL_STORAGE aunque su class no sea
    // BCTYPE_STORAGE. Por eso el filtro correcto es la funcion
    // de almacenamiento + ownership real del jugador.
    //

    if (
        function != BF_GENERAL_STORAGE &&
        function != BF_RESOURCE_STORAGE &&
        static_cast<int>(function) != 10
    )
    {
        return false;
    }


    Inventory* inventory =
        building->getInventory();


    if (
        !inventory ||
        inventory->getNumItems() <= 0
    )
    {
        return false;
    }


    return true;
}


static void BuildBetterLootingTargetQueue(
    Character* centerCharacter,
    std::vector<BetterLootingRadiusTarget>& queue,
    Inventory* excludedInventory
)
{
    queue.clear();


    if (
        !centerCharacter ||
        !ou
    )
    {
        return;
    }


    lektor<RootObject*> characterTargets;
    lektor<RootObject*> buildingTargets;


    CollectBetterLootingRadiusTargets(
        centerCharacter,
        characterTargets,
        buildingTargets
    );


    //
    // Crear conjunto con TODOS los personajes del jugador.
    //

    std::unordered_set<RootObject*> seenTargets;


    //
    // PERSONAJES:
    // solo muertos o inconscientes,
    // nunca miembros del jugador.
    //

    for (
        uint32_t i = 0;
        i < characterTargets.size();
        ++i
    )
    {
        RootObject* target =
            characterTargets[i];


        if (
            !target ||
            target == centerCharacter ||
            seenTargets.find(target) !=
                seenTargets.end()
        )
        {
            continue;
        }


        if (
            excludedInventory &&
            target->getInventory() ==
                excludedInventory
        )
        {
            BlTrace(
                "BetterLooting: RADIAL_TARGET SKIP direct target inventory"
            );

            continue;
        }


        Character* character =
            static_cast<Character*>(
                target
            );


        if (
            !IsBetterLootingCharacterTarget(character)
        )
        {
            continue;
        }


        if (
            !IsBetterLootingInsideConfiguredRadius(
                target,
                CHARACTER,
                centerCharacter
            )
        )
        {
            BlTrace(
                "BetterLooting: RADIAL_TARGET REJECT character outside strict radius"
            );

            continue;
        }


        seenTargets.insert(target);


        BetterLootingRadiusTarget entry;

        entry.object = target;
        entry.type = CHARACTER;

        queue.push_back(entry);
    }


    //
    // EDIFICIOS:
    // por ahora solo inventario válido con contenido.
    //

    for (
        uint32_t i = 0;
        i < buildingTargets.size();
        ++i
    )
    {
        RootObject* target =
            buildingTargets[i];


        if (
            !target ||
            seenTargets.find(target) !=
                seenTargets.end()
        )
        {
            continue;
        }


        if (
            excludedInventory &&
            target->getInventory() ==
                excludedInventory
        )
        {
            BlTrace(
                "BetterLooting: RADIAL_TARGET SKIP direct target inventory"
            );

            continue;
        }


        if (
            !IsBetterLootingBuildingTarget(
                target
            )
        )
        {
            continue;
        }


        if (
            !IsBetterLootingInsideConfiguredRadius(
                target,
                BUILDING,
                centerCharacter
            )
        )
        {
            BlTrace(
                "BetterLooting: RADIAL_TARGET REJECT building outside strict world radius"
            );

            continue;
        }


        seenTargets.insert(target);


        BetterLootingRadiusTarget entry;

        entry.object = target;
        entry.type = BUILDING;

        queue.push_back(entry);
    }


    //
    // One INSERT: nearest valid targets first.
    //

    const Ogre::Vector3 queueCenterPosition =
        centerCharacter->getPosition();


    std::stable_sort(
        queue.begin(),
        queue.end(),
        [queueCenterPosition](
            const BetterLootingRadiusTarget& left,
            const BetterLootingRadiusTarget& right
        ) -> bool
        {
            if (!left.object)
                return false;

            if (!right.object)
                return true;


            const float leftDistance =
                (
                    GetBetterLootingObjectWorldPosition(
                        left.object,
                        left.type
                    ) -
                    queueCenterPosition
                ).length();


            const float rightDistance =
                (
                    GetBetterLootingObjectWorldPosition(
                        right.object,
                        right.type
                    ) -
                    queueCenterPosition
                ).length();


            return
                leftDistance <
                rightDistance;
        }
    );


    BlTrace(
        "BetterLooting: RADIAL_QUEUE SORTED_NEAREST_FIRST"
    );
}



static std::vector<BetterLootingRadiusTarget>
    g_radialLootQueue;

static std::size_t g_radialLootIndex = 0;

static InventoryGUI*
    g_radialLootDestination = NULL;

static int g_radialLootWaitFrames = 0;

static bool g_radialLootActive = false;

static Character* g_radialLootCenterCharacter = NULL;


static void FinishBetterLootingRadiusRun()
{
    char message[256];

    sprintf_s(
        message,
        "BetterLooting: RADIAL_RUN END processed=%u total=%u",
        static_cast<unsigned int>(
            g_radialLootIndex
        ),
        static_cast<unsigned int>(
            g_radialLootQueue.size()
        )
    );

    BlTrace(message);


    g_radialLootQueue.clear();

    g_radialLootIndex = 0;

    g_radialLootDestination = NULL;

    g_radialLootWaitFrames = 0;

    g_radialLootCenterCharacter = NULL;

    g_radialLootActive = false;
}


static bool StartBetterLootingRadiusRun(
    InventoryGUI* destinationWindow,
    Character* centerCharacter,
    Inventory* excludedInventory
)
{
    if (g_betterLootingTransferFault)
    {
        BlTrace(
            "BetterLooting: RADIAL_RUN RESET previous transfer invariant fault"
        );

        g_betterLootingTransferFault = false;
    }


    if (
        !destinationWindow ||
        !centerCharacter ||
        g_radialLootActive
    )
    {
        return false;
    }


    g_radialLootQueue.clear();


    BuildBetterLootingTargetQueue(
        centerCharacter,
        g_radialLootQueue,
        excludedInventory
    );


    if (g_radialLootQueue.empty())
    {
        BlTrace(
            "BetterLooting: RADIAL_RUN EMPTY"
        );

        return false;
    }


    unsigned int characterCount = 0;
    unsigned int buildingCount = 0;


    for (
        std::size_t i = 0;
        i < g_radialLootQueue.size();
        ++i
    )
    {
        if (
            g_radialLootQueue[i].type ==
            CHARACTER
        )
        {
            ++characterCount;
        }
        else if (
            g_radialLootQueue[i].type ==
            BUILDING
        )
        {
            ++buildingCount;
        }
    }


    char rulesAudit[1024];

    sprintf_s(
        rulesAudit,
        "BetterLooting: AUDIT_RULES radius=%.2f minValue=%d minValuePerKg=%.2f allowStolen=%d weapons=%d armour=%d food=%d medicine=%d tools=%d materials=%d blueprints=%d ammo=%d robotics=%d books=%d narcotics=%d severedLimbs=%d other=%d bestItemsFirst=%d preferBackpack=%d activationVK=%d",
        g_betterLootingRules.lootRadius,
        g_betterLootingRules.minValue,
        g_betterLootingRules.minValuePerKg,
        g_betterLootingRules.allowStolen ? 1 : 0,
        g_betterLootingRules.lootWeapons ? 1 : 0,
        g_betterLootingRules.lootArmour ? 1 : 0,
        g_betterLootingRules.lootFood ? 1 : 0,
        g_betterLootingRules.lootMedicine ? 1 : 0,
        g_betterLootingRules.lootTools ? 1 : 0,
        g_betterLootingRules.lootMaterials ? 1 : 0,
        g_betterLootingRules.lootBlueprints ? 1 : 0,
        g_betterLootingRules.lootAmmo ? 1 : 0,
        g_betterLootingRules.lootRobotics ? 1 : 0,
        g_betterLootingRules.lootBooks ? 1 : 0,
        g_betterLootingRules.lootNarcotics ? 1 : 0,
        g_betterLootingRules.lootSeveredLimbs ? 1 : 0,
        g_betterLootingRules.lootOther ? 1 : 0,
        g_betterLootingRules.bestItemsFirst ? 1 : 0,
        g_betterLootingRules.preferBackpack ? 1 : 0,
        g_betterLootingRules.activationVirtualKey
    );

    BlTrace(rulesAudit);


    g_radialLootIndex = 0;

    g_radialLootDestination =
        destinationWindow;

    g_radialLootWaitFrames = 1;

    g_radialLootCenterCharacter =
        centerCharacter;

    g_radialLootActive = true;


    char message[384];

    sprintf_s(
        message,
        "BetterLooting: RADIAL_RUN START radius=%.2f targets=%u characters=%u buildings=%u",
        g_betterLootingRules.lootRadius,
        static_cast<unsigned int>(
            g_radialLootQueue.size()
        ),
        characterCount,
        buildingCount
    );

    BlTrace(message);


    return true;
}


static void ProcessBetterLootingRadiusRun()
{
    if (!g_radialLootActive)
    {
        return;
    }


    if (!g_radialLootDestination)
    {
        BlTrace(
            "BetterLooting: RADIAL_RUN ABORT invalid destination"
        );

        FinishBetterLootingRadiusRun();

        return;
    }


    if (g_radialLootWaitFrames > 0)
    {
        --g_radialLootWaitFrames;

        return;
    }


    if (
        g_radialLootIndex >=
        g_radialLootQueue.size()
    )
    {
        FinishBetterLootingRadiusRun();

        return;
    }


    const std::size_t currentIndex =
        g_radialLootIndex;


    BetterLootingRadiusTarget target =
        g_radialLootQueue[currentIndex];


    ++g_radialLootIndex;


    if (!target.object)
    {
        BlTrace(
            "BetterLooting: RADIAL_TARGET SKIP null object"
        );

        g_radialLootWaitFrames = 1;

        return;
    }


    if (
        !g_radialLootCenterCharacter ||
        !IsBetterLootingInsideConfiguredRadius(
            target.object,
            target.type,
            g_radialLootCenterCharacter
        )
    )
    {
        BlTrace(
            "BetterLooting: RADIAL_TARGET SKIP live strict radius reject"
        );

        g_radialLootWaitFrames = 1;

        return;
    }


    Inventory* inventory =
        target.object->getInventory();


    if (
        !inventory ||
        inventory->getNumItems() <= 0
    )
    {
        char skipMessage[256];

        sprintf_s(
            skipMessage,
            "BetterLooting: RADIAL_TARGET index=%u result=SKIP_EMPTY",
            static_cast<unsigned int>(
                currentIndex
            )
        );

        BlTrace(skipMessage);


        g_radialLootWaitFrames = 1;


        if (
            g_radialLootIndex >=
            g_radialLootQueue.size()
        )
        {
            FinishBetterLootingRadiusRun();
        }

        return;
    }


    char targetMessage[384];

    sprintf_s(
        targetMessage,
        "BetterLooting: RADIAL_TARGET index=%u type=%s inventory=%p items=%d",
        static_cast<unsigned int>(
            currentIndex
        ),
        target.type == CHARACTER
            ? "CHARACTER"
            : "BUILDING",
        inventory,
        inventory->getNumItems()
    );

    BlTrace(targetMessage);


    EvaluateTargetInventory(
        NULL,
        inventory,
        g_radialLootDestination
    );


    if (g_betterLootingTransferFault)
    {
        BlTrace(
            "BetterLooting: RADIAL_RUN ABORT transfer invariant fault"
        );

        FinishBetterLootingRadiusRun();

        return;
    }


    g_radialLootWaitFrames = 1;


    if (
        g_radialLootIndex >=
        g_radialLootQueue.size()
    )
    {
        FinishBetterLootingRadiusRun();
    }
}


// ============================================================
// BL_FEATURE_STORE_ALL_SHIFT_P_SAFE_ROUTING
//
// "GUARDAR TODO / SAVE ALL" is requested by the Win32 UI thread,
// but all Kenshi inventory/building work runs here from the game
// InventoryGUI update hook.
//
// Rules:
// - source: player backpack + player main inventory, non-equipment sections only
// - filter: the same BetterLooting rules/categories currently saved from SHIFT+P
// - destination: player-owned BF_GENERAL_STORAGE first, then BF_RESOURCE_STORAGE
// - empty storages are valid destinations
// - placement is resolved per Item + destination InventorySection
// - transfer is one unit per transaction, with exact -1/+1 conservation checks
// - if destination did not change, rollback returns that one unit to the exact source section
// - equipment sections are never used as source or destination
// ============================================================

static volatile LONG g_storeAllRequested = 0;
static bool g_storeAllRunning = false;
static bool g_storeAllTransferFault = false;


struct BetterLootingStoreTarget
{
    RootObject* object;
    Inventory* inventory;
    BuildingFunction function;
    float distance;
    Character* centerCharacter;
    Faction* playerFaction;
};


struct BetterLootingStorePlacement
{
    RootObject* object;
    Inventory* inventory;
    InventorySection* section;
    BuildingFunction function;
    int maxStack;
    bool exactResourceMatch;
    bool found;
};


static bool IsBetterLootingPlayerOwnedBuildingChain(
    Building* building,
    Faction* playerFaction
)
{
    if (
        !building ||
        !playerFaction ||
        !playerFaction->isThePlayer()
    )
    {
        return false;
    }


    Building* current =
        building;


    for (int depth = 0; depth < 8; ++depth)
    {
        if (
            !current ||
            !current->isValid() ||
            !current->isThePlayer()
        )
        {
            return false;
        }


        Faction* currentFaction =
            current->getFaction();


        if (
            !currentFaction ||
            currentFaction != playerFaction ||
            !currentFaction->isThePlayer()
        )
        {
            return false;
        }


        Building* parent =
            current->
                furnitureParentBuilding();


        if (!parent)
        {
            return true;
        }


        if (parent == current)
        {
            return false;
        }


        current =
            parent;
    }


    return false;
}


static bool BetterLootingStoreInventoryHasSpecializedSection(
    Inventory* inventory
)
{
    if (!inventory)
    {
        return false;
    }


    lektor<InventorySection*>& sections =
        inventory->getAllSections();


    for (
        uint32_t sectionIndex = 0;
        sectionIndex <
            static_cast<uint32_t>(
                sections.size()
            );
        ++sectionIndex
    )
    {
        InventorySection* section =
            sections[sectionIndex];


        if (!section)
        {
            continue;
        }


        if (!section->getEnabled())
        {
            continue;
        }


        if (section->containerSlot)
        {
            continue;
        }


        if (
            section->
                isAnEquippedItemSection
        )
        {
            continue;
        }


        if (
            section->
                getVeryLimitedSlot().
                size() > 0
        )
        {
            return true;
        }
    }


    return false;
}


static bool IsBetterLootingStoreBuildingTarget(
    RootObject* object,
    Faction* playerFaction
)
{
    if (
        !object ||
        !playerFaction
    )
    {
        return false;
    }


    Building* building =
        static_cast<Building*>(
            object
        );


    if (
        !IsBetterLootingPlayerOwnedBuildingChain(
            building,
            playerFaction
        )
    )
    {
        return false;
    }


    const BuildingFunction function =
        building->getSpecialFunction();


    Inventory* inventory =
        building->getInventory();


    if (!inventory)
    {
        return false;
    }


    return
        function == BF_GENERAL_STORAGE ||
        function == BF_RESOURCE_STORAGE;
}


static bool IsBetterLootingStoreTargetLiveValid(
    const BetterLootingStoreTarget& target
)
{
    if (
        !target.object ||
        !target.centerCharacter ||
        !target.playerFaction
    )
    {
        return false;
    }


    if (
        !target.centerCharacter->
            isPlayerCharacter()
    )
    {
        return false;
    }


    Faction* livePlayerFaction =
        target.centerCharacter->
            getFaction();


    if (
        !livePlayerFaction ||
        livePlayerFaction !=
            target.playerFaction ||
        !livePlayerFaction->
            isThePlayer()
    )
    {
        return false;
    }


    if (
        !IsBetterLootingStoreBuildingTarget(
            target.object,
            target.playerFaction
        )
    )
    {
        return false;
    }


    Building* building =
        static_cast<Building*>(
            target.object
        );


    Inventory* liveInventory =
        building->getInventory();


    if (
        !liveInventory ||
        liveInventory !=
            target.inventory
    )
    {
        return false;
    }


    float liveDistance = -1.0f;


    return
        GetBetterLootingStoreStrictDistance(
            building,
            target.centerCharacter,
            liveDistance
        );
}


static Inventory* GetBetterLootingPreferredOriginInsideRadius(
    Inventory* playerInventory,
    GameData* itemData,
    const std::vector<BetterLootingStoreTarget>& targets
)
{
    Inventory* preferredOrigin =
        GetBetterLootingPreferredOriginInventory(
            playerInventory,
            itemData
        );


    if (!preferredOrigin)
    {
        return NULL;
    }


    for (
        std::size_t i = 0;
        i < targets.size();
        ++i
    )
    {
        const BetterLootingStoreTarget& target =
            targets[i];


        if (
            target.inventory == preferredOrigin &&
            IsBetterLootingStoreTargetLiveValid(
                target
            )
        )
        {
            return preferredOrigin;
        }
    }


    BlTrace(
        "BetterLooting: STORE_ALL ORIGIN_REJECT outside current strict radius or no longer valid"
    );


    return NULL;
}


static void BuildBetterLootingStoreTargetQueue(
    Character* centerCharacter,
    std::vector<BetterLootingStoreTarget>& targets
)
{
    targets.clear();


    if (
        !centerCharacter ||
        !ou
    )
    {
        return;
    }


    Faction* playerFaction =
        centerCharacter->
            getFaction();


    if (
        !playerFaction ||
        !playerFaction->
            isThePlayer()
    )
    {
        BlTrace(
            "BetterLooting: STORE_ALL ABORT no exact player faction"
        );

        return;
    }


    lektor<RootObject*> characterTargets;
    lektor<RootObject*> buildingTargets;


    CollectBetterLootingRadiusTargets(
        centerCharacter,
        characterTargets,
        buildingTargets
    );


    const float radiusInternal =
        g_betterLootingRules.lootRadius *
        10.0f;


    for (
        uint32_t i = 0;
        i < buildingTargets.size();
        ++i
    )
    {
        RootObject* object =
            buildingTargets[i];


        if (
            !IsBetterLootingStoreBuildingTarget(
                object,
                playerFaction
            )
        )
        {
            continue;
        }


        Building* building =
            static_cast<Building*>(
                object
            );


        float distance = -1.0f;


        if (
            !GetBetterLootingStoreStrictDistance(
                building,
                centerCharacter,
                distance
            )
        )
        {
            char radiusMessage[512];

            sprintf_s(
                radiusMessage,
                "BetterLooting: STORE_ALL TARGET_REJECT strict_radius_or_town object=%p radiusInternal=%.2f radiusMeters=%.2f",
                object,
                radiusInternal,
                g_betterLootingRules.lootRadius
            );

            BlTrace(
                radiusMessage
            );

            continue;
        }


        Inventory* inventory =
            building->getInventory();


        if (!inventory)
        {
            continue;
        }


        BetterLootingStoreTarget target;

        target.object =
            object;

        target.inventory =
            inventory;

        target.function =
            building->getSpecialFunction();

        target.distance =
            distance;

        target.centerCharacter =
            centerCharacter;

        target.playerFaction =
            playerFaction;


        targets.push_back(
            target
        );


        char acceptedMessage[768];

        sprintf_s(
            acceptedMessage,
            "BetterLooting: STORE_ALL TARGET_ACCEPT object=%p inventory=%p function=%d specializedInventory=%d distanceInternal=%.2f distanceMeters=%.2f playerFaction=%p parent=%p",
            object,
            inventory,
            static_cast<int>(
                target.function
            ),
            BetterLootingStoreInventoryHasSpecializedSection(
                inventory
            ) ? 1 : 0,
            target.distance,
            target.distance / 10.0f,
            playerFaction,
            building->
                furnitureParentBuilding()
        );

        BlTrace(
            acceptedMessage
        );
    }


    std::sort(
        targets.begin(),
        targets.end(),
        [](
            const BetterLootingStoreTarget& left,
            const BetterLootingStoreTarget& right
        ) -> bool
        {
            const int leftPriority =
                left.function ==
                    BF_GENERAL_STORAGE
                    ? 0
                    : 1;

            const int rightPriority =
                right.function ==
                    BF_GENERAL_STORAGE
                    ? 0
                    : 1;


            if (left.distance != right.distance)
            {
                return
                    left.distance <
                    right.distance;
            }


            return
                leftPriority <
                rightPriority;
        }
    );


    char message[512];

    sprintf_s(
        message,
        "BetterLooting: STORE_ALL TARGETS_PLAYER_ONLY radiusMeters=%.2f radiusInternal=%.2f count=%u playerFaction=%p",
        g_betterLootingRules.lootRadius,
        radiusInternal,
        static_cast<unsigned int>(
            targets.size()
        ),
        playerFaction
    );

    BlTrace(message);
}


static bool BetterLootingStoreSectionHasExactResource(
    InventorySection* section,
    GameData* itemData
)
{
    if (
        !section ||
        !itemData
    )
    {
        return false;
    }


    const lektor<GameData*>& exactItems =
        section->
            getVeryLimitedSlot();


    for (
        uint32_t i = 0;
        i <
            static_cast<uint32_t>(
                exactItems.size()
            );
        ++i
    )
    {
        if (
            exactItems[i] ==
            itemData
        )
        {
            return true;
        }
    }


    return false;
}


static BetterLootingStorePlacement
FindBetterLootingStorePlacement(
    const std::vector<BetterLootingStoreTarget>& targets,
    Item* item,
    GameData* itemData,
    bool resourceItem,
    Inventory* preferredOriginInventory
)
{
    BetterLootingStorePlacement result = {
        nullptr,
        nullptr,
        nullptr,
        static_cast<BuildingFunction>(0),
        0,
        false,
        false
    };


    if (
        !item ||
        !itemData
    )
    {
        return result;
    }


    //
    // Safe destination policy:
    //
    // Pass ORIGIN:
    //   Return to the exact inventory from which INSERT looted this unit,
    //   but only if that inventory is currently present in the validated
    //   PLAYER storage target queue, inside radius and compatible.
    //
    // Pass FALLBACK:
    //   1. Nearest compatible PLAYER storage inside the current strict radius.
    //   2. Distance ties keep BF_GENERAL_STORAGE before BF_RESOURCE_STORAGE.
    //   3. No destination -> item remains in the PLAYER source inventory.
    //
    // Refineries, crafting, research, machines, NPC inventories, corpses,
    // traders and non-PLAYER buildings can never become STORE_ALL targets.
    //

    const int selectionPassCount =
        preferredOriginInventory
            ? 2
            : 1;


    for (
        int selectionPass = 0;
        selectionPass < selectionPassCount;
        ++selectionPass
    )
    {
        const bool originOnlyPass =
            preferredOriginInventory &&
            selectionPass == 0;


        for (
            std::size_t targetIndex = 0;
            targetIndex < targets.size();
            ++targetIndex
        )
        {
            const BetterLootingStoreTarget& target =
                targets[targetIndex];


            if (
                target.function !=
                    BF_GENERAL_STORAGE &&
                target.function !=
                    BF_RESOURCE_STORAGE
            )
            {
                continue;
            }


            const bool targetIsPreferredOrigin =
                preferredOriginInventory &&
                target.inventory ==
                    preferredOriginInventory;


            if (
                preferredOriginInventory
            )
            {
                if (
                    originOnlyPass &&
                    !targetIsPreferredOrigin
                )
                {
                    continue;
                }


                if (
                    !originOnlyPass &&
                    targetIsPreferredOrigin
                )
                {
                    continue;
                }
            }


            if (
                !IsBetterLootingStoreTargetLiveValid(
                    target
                )
            )
            {
                char rejectMessage[512];

                sprintf_s(
                    rejectMessage,
                    "BetterLooting: STORE_ALL LIVE_TARGET_REJECT index=%u object=%p cachedInventory=%p",
                    static_cast<unsigned int>(
                        targetIndex
                    ),
                    target.object,
                    target.inventory
                );

                BlTrace(
                    rejectMessage
                );

                continue;
            }


            Building* building =
                static_cast<Building*>(
                    target.object
                );


            Inventory* inventory =
                building->getInventory();


            if (
                !inventory ||
                inventory !=
                    target.inventory
            )
            {
                continue;
            }


            lektor<InventorySection*>& sections =
                inventory->getAllSections();


            for (
                uint32_t sectionIndex = 0;
                sectionIndex <
                    static_cast<uint32_t>(
                        sections.size()
                    );
                ++sectionIndex
            )
            {
                InventorySection* section =
                    sections[sectionIndex];


                if (!section)
                {
                    continue;
                }


                if (!section->getEnabled())
                {
                    continue;
                }


                if (section->containerSlot)
                {
                    continue;
                }


                if (
                    section->
                        isAnEquippedItemSection
                )
                {
                    continue;
                }


                const bool exactResourceMatch =
                    BetterLootingStoreSectionHasExactResource(
                        section,
                        itemData
                    );


                bool compatible =
                    section->
                        isLimitedSlotCompatible(
                            item
                        );


                if (
                    !compatible &&
                    resourceItem &&
                    target.function ==
                        BF_RESOURCE_STORAGE
                )
                {
                    compatible =
                        section->
                            isLimitedSlotCompatible(
                                itemData
                            );
                }


                if (!compatible)
                {
                    continue;
                }


                if (
                    !section->
                        hasRoomForItem(
                            itemData,
                            1
                        )
                )
                {
                    continue;
                }


                int x = 0;
                int y = 0;


                if (
                    !section->
                        getValidInventoryPosition(
                            item,
                            x,
                            y
                        )
                )
                {
                    continue;
                }


                int maxStack =
                    item->
                        isStackable(
                            section
                        );


                if (maxStack <= 0)
                {
                    maxStack = 1;
                }


                result.object =
                    target.object;

                result.inventory =
                    inventory;

                result.section =
                    section;

                result.function =
                    target.function;

                result.maxStack =
                    maxStack;

                result.exactResourceMatch =
                    exactResourceMatch;

                result.found =
                    true;


                const char* tier =
                    targetIsPreferredOrigin
                        ? "ORIGIN"
                        : (
                            target.function ==
                                BF_GENERAL_STORAGE
                                ? "GENERAL"
                                : "RESOURCE"
                        );


                char selectMessage[1152];

                sprintf_s(
                    selectMessage,
                    "BetterLooting: STORE_ALL DESTINATION_SELECT tier=%s resource=%d exactResource=%d targetIndex=%u object=%p inventory=%p preferredOrigin=%p section=%p sectionIndex=%u function=%d distanceMeters=%.2f",
                    tier,
                    resourceItem ? 1 : 0,
                    exactResourceMatch ? 1 : 0,
                    static_cast<unsigned int>(
                        targetIndex
                    ),
                    target.object,
                    inventory,
                    preferredOriginInventory,
                    section,
                    static_cast<unsigned int>(
                        sectionIndex
                    ),
                    static_cast<int>(
                        target.function
                    ),
                    target.distance / 10.0f
                );

                BlTrace(
                    selectMessage
                );


                return result;
            }
        }
    }


    return result;
}


static std::vector<BetterLootingCandidate>
EnumerateBetterLootingStoreSourceCandidates(
    Inventory* inventory
)
{
    std::vector<BetterLootingCandidate> filtered;


    const std::vector<BetterLootingCandidate> all =
        EnumerateInventoryCandidates(
            inventory
        );


    filtered.reserve(
        all.size()
    );


    for (
        std::size_t i = 0;
        i < all.size();
        ++i
    )
    {
        const BetterLootingCandidate& candidate =
            all[i];


        if (
            !candidate.item ||
            !candidate.section
        )
        {
            continue;
        }


        if (
            candidate.section->
                isAnEquippedItemSection
        )
        {
            continue;
        }


        if (
            candidate.section->
                containerSlot
        )
        {
            continue;
        }


        filtered.push_back(
            candidate
        );
    }


    return filtered;
}


static void ReleaseBetterLootingProtectedLootForStore(
    Inventory* sourceInventory,
    GameData* itemData,
    int quantity
)
{
    if (
        !sourceInventory ||
        !itemData ||
        quantity <= 0
    )
    {
        return;
    }


    Character* character =
        sourceInventory->
            getCallbackCharacter();


    if (
        !character ||
        !character->isPlayerCharacter()
    )
    {
        return;
    }


    hand characterHandle(
        character
    );


    const int index =
        FindBetterLootingProtectionIndex(
            characterHandle,
            sourceInventory,
            itemData
        );


    if (index < 0)
    {
        return;
    }


    BetterLootingProtectedLoot& entry =
        g_betterLootingProtectedLoot[
            static_cast<std::size_t>(
                index
            )
        ];


    const int before =
        entry.protectedQuantity;


    entry.protectedQuantity -=
        quantity;


    if (
        entry.protectedQuantity <
        0
    )
    {
        entry.protectedQuantity =
            0;
    }


    char message[512];

    sprintf_s(
        message,
        "BetterLooting: STORE_ALL PROTECTION_RELEASE inventory=%p gameData=%p before=%d released=%d after=%d",
        sourceInventory,
        itemData,
        before,
        quantity,
        entry.protectedQuantity
    );

    BlTrace(message);
}


static void LogBetterLootingStoreInvariantFault(
    GameData* itemData,
    int sourceBefore,
    int sourceAfter,
    int destinationBefore,
    int destinationAfter
)
{
    char message[768];

    sprintf_s(
        message,
        "BetterLooting: STORE_ALL INVARIANT_FAULT gameData=%p source=%d->%d destination=%d->%d STORE_ALL_DISABLED=1",
        itemData,
        sourceBefore,
        sourceAfter,
        destinationBefore,
        destinationAfter
    );

    BlTrace(message);


    g_storeAllTransferFault =
        true;
}


static void ExecuteBetterLootingStoreCandidate(
    Inventory* sourceInventory,
    const std::vector<BetterLootingStoreTarget>& targets,
    const BetterLootingCandidate& candidate,
    const char* sourceName,
    bool resourceItem
)
{
    if (
        g_storeAllTransferFault ||
        !sourceInventory ||
        !candidate.item
    )
    {
        return;
    }


    Item* sourceItem =
        candidate.item;


    GameData* itemData =
        sourceItem->
            getGameData();


    if (!itemData)
    {
        return;
    }


    int movedTotal = 0;


    for (
        int transferPass = 0;
        transferPass < 4096;
        ++transferPass
    )
    {
        BetterLootingItemLocation liveLocation =
            FindItemLocation(
                sourceInventory,
                sourceItem
            );


        if (!liveLocation.found)
        {
            return;
        }


        if (
            !liveLocation.section ||
            liveLocation.section->
                isAnEquippedItemSection ||
            liveLocation.section->
                containerSlot
        )
        {
            return;
        }


        const int sourceQuantity =
            GetBetterLootingItemQuantity(
                sourceItem
            );


        if (
            sourceQuantity <= 0 ||
            sourceQuantity > 100000
        )
        {
            BlTrace(
                "BetterLooting: STORE_ALL invalid source quantity"
            );

            return;
        }


        Inventory* preferredOriginInventory =
            GetBetterLootingPreferredOriginInsideRadius(
                sourceInventory,
                itemData,
                targets
            );


        BetterLootingStorePlacement placement =
            FindBetterLootingStorePlacement(
                targets,
                sourceItem,
                itemData,
                resourceItem,
                preferredOriginInventory
            );


        if (!placement.found)
        {
            char noTargetMessage[512];

            sprintf_s(
                noTargetMessage,
                "BetterLooting: STORE_ALL NO_DESTINATION source=%s gameData=%p remaining=%d moved=%d",
                sourceName,
                itemData,
                sourceQuantity,
                movedTotal
            );

            BlTrace(
                noTargetMessage
            );

            return;
        }


        const int sourceBefore =
            sourceInventory->
                countItems(
                    itemData
                );

        const int destinationBefore =
            placement.inventory->
                countItems(
                    itemData
                );


        const bool sourceWillRemain =
            sourceQuantity > 1;


        Item* detachedItem =
            sourceInventory->
                removeItemDontDestroy_returnsItem(
                    sourceItem,
                    1,
                    sourceWillRemain
                );


        if (!detachedItem)
        {
            BlTrace(
                "BetterLooting: STORE_ALL source remove failed"
            );

            return;
        }


        const int detachedQuantity =
            GetBetterLootingItemQuantity(
                detachedItem
            );


        if (detachedQuantity != 1)
        {
            LogBetterLootingStoreInvariantFault(
                itemData,
                sourceBefore,
                sourceInventory->
                    countItems(
                        itemData
                    ),
                destinationBefore,
                placement.inventory->
                    countItems(
                        itemData
                    )
            );

            return;
        }


        const bool engineAdded =
            placement.section->
                addItem(
                    detachedItem,
                    1
                );


        sourceInventory->
            notifyModified();

        placement.inventory->
            notifyModified();


        int sourceAfter =
            sourceInventory->
                countItems(
                    itemData
                );

        int destinationAfter =
            placement.inventory->
                countItems(
                    itemData
                );


        const int sourceDelta =
            sourceBefore -
            sourceAfter;

        const int destinationDelta =
            destinationAfter -
            destinationBefore;


        if (
            sourceDelta == 1 &&
            destinationDelta == 1
        )
        {
            ReleaseBetterLootingProtectedLootForStore(
                sourceInventory,
                itemData,
                1
            );


            ReleaseBetterLootingLootOriginForStore(
                sourceInventory,
                itemData,
                1
            );


            ++movedTotal;


            char successMessage[768];

            sprintf_s(
                successMessage,
                "BetterLooting: STORE_ALL MOVE_OK source=%s pass=%d gameData=%p destinationObject=%p destinationInventory=%p destinationFunction=%d exactResource=%d quantity=1 movedTotal=%d source=%d->%d destination=%d->%d destinationMaxStack=%d engineReturn=%d",
                sourceName,
                transferPass,
                itemData,
                placement.object,
                placement.inventory,
                static_cast<int>(
                    placement.function
                ),
                placement.exactResourceMatch ? 1 : 0,
                movedTotal,
                sourceBefore,
                sourceAfter,
                destinationBefore,
                destinationAfter,
                placement.maxStack,
                engineAdded ? 1 : 0
            );

            BlTrace(
                successMessage
            );


            if (!sourceWillRemain)
            {
                return;
            }


            if (
                !sourceInventory->
                    hasItem(
                        sourceItem
                    )
            )
            {
                LogBetterLootingStoreInvariantFault(
                    itemData,
                    sourceBefore,
                    sourceInventory->
                        countItems(
                            itemData
                        ),
                    destinationBefore,
                    placement.inventory->
                        countItems(
                            itemData
                        )
                );

                return;
            }


            continue;
        }


        if (destinationDelta == 0)
        {
            const bool rollbackReported =
                liveLocation.section->
                    addItem(
                        detachedItem,
                        1
                    );


            sourceInventory->
                notifyModified();

            placement.inventory->
                notifyModified();


            const int sourceAfterRollback =
                sourceInventory->
                    countItems(
                        itemData
                    );

            const int destinationAfterRollback =
                placement.inventory->
                    countItems(
                        itemData
                    );


            const bool rollbackExact =
                sourceAfterRollback ==
                    sourceBefore &&
                destinationAfterRollback ==
                    destinationBefore;


            char rollbackMessage[768];

            sprintf_s(
                rollbackMessage,
                "BetterLooting: STORE_ALL ROLLBACK engineReturn=%d rollbackReturn=%d exact=%d source=%d->%d destination=%d->%d",
                engineAdded ? 1 : 0,
                rollbackReported ? 1 : 0,
                rollbackExact ? 1 : 0,
                sourceBefore,
                sourceAfterRollback,
                destinationBefore,
                destinationAfterRollback
            );

            BlTrace(
                rollbackMessage
            );


            if (rollbackExact)
            {
                return;
            }


            sourceAfter =
                sourceAfterRollback;

            destinationAfter =
                destinationAfterRollback;
        }


        LogBetterLootingStoreInvariantFault(
            itemData,
            sourceBefore,
            sourceAfter,
            destinationBefore,
            destinationAfter
        );

        return;
    }


    BlTrace(
        "BetterLooting: STORE_ALL safety pass limit"
    );


    g_storeAllTransferFault =
        true;
}


static void EvaluateBetterLootingStoreSourceInventory(
    Inventory* inventory,
    const std::vector<BetterLootingStoreTarget>& targets,
    const char* sourceName
)
{
    if (
        !inventory ||
        g_storeAllTransferFault
    )
    {
        return;
    }


    ogre_unordered_set<GameData*>::type
        resourceItems;


    inventory->
        getResourceItems(
            resourceItems,
            true
        );


    const std::vector<BetterLootingCandidate> candidates =
        EnumerateBetterLootingStoreSourceCandidates(
            inventory
        );


    char startMessage[384];

    sprintf_s(
        startMessage,
        "BetterLooting: STORE_ALL SOURCE_START source=%s inventory=%p candidates=%u",
        sourceName,
        inventory,
        static_cast<unsigned int>(
            candidates.size()
        )
    );

    BlTrace(
        startMessage
    );


    for (
        std::size_t candidateIndex = 0;
        candidateIndex < candidates.size();
        ++candidateIndex
    )
    {
        const BetterLootingCandidate& candidate =
            candidates[
                candidateIndex
            ];


        Item* item =
            candidate.item;


        if (!item)
        {
            continue;
        }


        GameData* itemData =
            item->
                getGameData();


        const int valueSingle =
            item->
                getValueSingle(
                    true
                );

        const float weightSingle =
            item->
                getItemWeightSingle();

        const float valuePerKg =
            weightSingle > 0.001f
                ? static_cast<float>(
                    valueSingle
                ) /
                    weightSingle
                : 0.0f;

        const int type =
            static_cast<int>(
                item->
                    getItemType()
            );

        const bool stolen =
            item->
                isStolen(
                    true
                );

        const int itemFunctionValue =
            GetBetterLootingItemFunctionValue(
                item
            );

        const bool isResearchArtifact =
            item->isResearchArtifact();

        const int effectiveItemFunctionValue =
            GetBetterLootingEffectiveItemFunctionValue(
                item,
                itemFunctionValue
            );

        const bool isResource =
            itemData &&
            resourceItems.find(
                itemData
            ) !=
                resourceItems.end();


        const BetterLootingDecision decision =
            EvaluateLootDecision(
                valueSingle,
                weightSingle,
                stolen,
                type,
                effectiveItemFunctionValue,
                isResource
            );


        char decisionMessage[640];

        sprintf_s(
            decisionMessage,
            "BetterLooting: STORE_ALL ITEM source=%s index=%u gameData=%p type=%d rawFunction=%d effectiveFunction=%d researchArtifact=%d resource=%d stolen=%d value=%d valuePerKg=%.2f decision=%s",
            sourceName,
            static_cast<unsigned int>(
                candidateIndex
            ),
            itemData,
            type,
            itemFunctionValue,
            effectiveItemFunctionValue,
            isResearchArtifact ? 1 : 0,
            isResource ? 1 : 0,
            stolen ? 1 : 0,
            valueSingle,
            valuePerKg,
            BetterLootingDecisionName(
                decision
            )
        );

        BlTrace(
            decisionMessage
        );


        if (
            decision !=
                BetterLootingDecision::LOOT
        )
        {
            continue;
        }


        ExecuteBetterLootingStoreCandidate(
            inventory,
            targets,
            candidate,
            sourceName,
            isResource
        );


        if (g_storeAllTransferFault)
        {
            BlTrace(
                "BetterLooting: STORE_ALL SOURCE_ABORT invariant fault"
            );

            return;
        }
    }
}


static Inventory*
GetBetterLootingValidatedBackpackInventory(
    Character* character
)
{
    if (
        !character ||
        !g_playerBackpackWindow
    )
    {
        return NULL;
    }


    Character* callback =
        g_playerBackpackWindow->
            getCallbackCharacter();


    if (
        callback != character ||
        !callback->
            isPlayerCharacter()
    )
    {
        return NULL;
    }


    return
        g_playerBackpackWindow->
            getInventory();
}


static void RunBetterLootingStoreAll(
    InventoryGUI* livePlayerWindow
)
{
    if (
        !livePlayerWindow ||
        g_storeAllRunning
    )
    {
        return;
    }


    Character* character =
        GetBetterLootingRadiusCenter(
            livePlayerWindow
        );


    if (!character)
    {
        BlTrace(
            "BetterLooting: STORE_ALL ABORT no live player character"
        );

        return;
    }


    g_storeAllRunning =
        true;

    g_storeAllTransferFault =
        false;


    std::vector<BetterLootingStoreTarget>
        targets;


    BuildBetterLootingStoreTargetQueue(
        character,
        targets
    );


    if (targets.empty())
    {
        BlTrace(
            "BetterLooting: STORE_ALL END no storage targets"
        );

        g_storeAllRunning =
            false;

        return;
    }


    Inventory* backpackInventory =
        GetBetterLootingValidatedBackpackInventory(
            character
        );


    Inventory* mainInventory =
        character->
            getInventory();


    char startMessage[512];

    sprintf_s(
        startMessage,
        "BetterLooting: STORE_ALL START character=%p backpack=%p main=%p targets=%u",
        character,
        backpackInventory,
        mainInventory,
        static_cast<unsigned int>(
            targets.size()
        )
    );

    BlTrace(
        startMessage
    );


    if (backpackInventory)
    {
        EvaluateBetterLootingStoreSourceInventory(
            backpackInventory,
            targets,
            "PLAYER_BACKPACK"
        );
    }


    if (
        !g_storeAllTransferFault &&
        mainInventory &&
        mainInventory !=
            backpackInventory
    )
    {
        EvaluateBetterLootingStoreSourceInventory(
            mainInventory,
            targets,
            "PLAYER_MAIN"
        );
    }


    char endMessage[384];

    sprintf_s(
        endMessage,
        "BetterLooting: STORE_ALL END fault=%d",
        g_storeAllTransferFault
            ? 1
            : 0
    );

    BlTrace(
        endMessage
    );


    g_storeAllRunning =
        false;
}


static bool g_smartLootInsertConsumed = false;


static void InventoryGUIUpdate_hook(
    InventoryGUI* self
)
{
    g_origInventoryGUIUpdate(self);


    //
    // Solo el InventoryGUI raíz procesa entrada.
    // Los hijos son mochila, contenedor, loot, etc.
    //

    if (
        !self ||
        self->ownerInventory != NULL
    )
    {
        return;
    }


    Character* storeTriggerCharacter =
        self->getCallbackCharacter();


    Inventory* storeTriggerInventory =
        self->getInventory();


    const bool storeTriggerIsPlayerMain =
        storeTriggerCharacter &&
        storeTriggerCharacter->
            isPlayerCharacter() &&
        storeTriggerInventory &&
        storeTriggerInventory ==
            storeTriggerCharacter->
                getInventory();


    if (
        InterlockedCompareExchange(
            &g_storeAllRequested,
            0,
            0
        ) != 0 &&
        storeTriggerIsPlayerMain &&
        !g_storeAllRunning &&
        !g_pendingLootRunning &&
        !g_pendingLootSource &&
        !g_pendingLootInventory &&
        !g_radialLootActive
    )
    {
        InterlockedExchange(
            &g_storeAllRequested,
            0
        );


        BlTrace(
            "BetterLooting: STORE_ALL PLAYER_INVENTORY_TRIGGER"
        );


        RunBetterLootingStoreAll(
            self
        );
    }


    const bool activationDown =
        (
            GetAsyncKeyState(
                g_betterLootingRules.activationVirtualKey
            ) & 0x8000
        ) != 0;


    if (!activationDown)
    {
        g_smartLootInsertConsumed = false;
    }


    //
    // El target ya fue clasificado por los hooks showInventory*.
    // Aquí el trigger se procesa por frame, no durante apertura GUI.
    //

    if (
        activationDown &&
        !g_smartLootInsertConsumed &&
        !g_pendingLootRunning &&
        !g_pendingLootSource &&
        !g_pendingLootInventory &&
        !g_radialLootActive
    )
    {
        Character* livePlayerCharacter =
            GetBetterLootingRadiusCenter(
                self
            );


        if (!livePlayerCharacter)
        {
            return;
        }


        Inventory* liveRootInventory =
            self->getInventory();


        Inventory* playerMainInventory =
            livePlayerCharacter->
                getInventory();


        if (
            !liveRootInventory ||
            liveRootInventory !=
                playerMainInventory
        )
        {
            BlTrace(
                "BetterLooting: SMART_LOOT PLAYER_INVENTORY_ONLY_REJECT"
            );

            return;
        }


        InventoryGUI* destination =
            SelectLootDestination();


        if (!destination)
        {
            BlTrace(
                "BetterLooting: SMART_LOOT RADIAL_ONLY_NO_DESTINATION"
            );

            g_smartLootInsertConsumed =
                true;

            return;
        }


        g_radialAfterDirectPending =
            false;

        g_radialAfterDirectExcludedInventory =
            NULL;


        const bool radialStarted =
            StartBetterLootingRadiusRun(
                destination,
                livePlayerCharacter,
                NULL
            );


        g_smartLootInsertConsumed =
            true;


        char triggerMessage[256];

        sprintf_s(
            triggerMessage,
            "BetterLooting: SMART_LOOT PLAYER_INVENTORY_RADIAL_TRIGGER started=%d",
            radialStarted ? 1 : 0
        );

        BlTrace(
            triggerMessage
        );
    }



    //
    ProcessBetterLootingRadiusRun();


    // Ejecutar transferencia pendiente.
    //

    if (
        g_pendingLootRunning ||
        !g_pendingLootInventory
    )
    {
        return;
    }


    if (g_pendingLootFrames > 0)
    {
        --g_pendingLootFrames;

        return;
    }


    Inventory* inventory =
        g_pendingLootInventory;

    InventoryGUI* destinationWindow =
        g_pendingLootDestination;


    g_pendingLootSource = NULL;
    g_pendingLootInventory = NULL;
    g_pendingLootDestination = NULL;

    g_lastLootTargetWindow = NULL;
    g_lastLootTargetInventory = NULL;

    g_pendingLootRunning = true;


    if (
        destinationWindow &&
        inventory
    )
    {
        BlTrace(
            "BetterLooting: SMART_LOOT DEFERRED_RUN"
        );

        EvaluateTargetInventory(
            NULL,
            inventory,
            destinationWindow
        );
    }
    else
    {
        BlTrace(
            "BetterLooting: SMART_LOOT DEFERRED_SKIP invalid state"
        );
    }


    g_pendingLootRunning = false;


    if (g_radialAfterDirectPending)
    {
        Inventory* excludedInventory =
            g_radialAfterDirectExcludedInventory;


        g_radialAfterDirectPending = false;

        g_radialAfterDirectExcludedInventory = NULL;


        if (g_betterLootingTransferFault)
        {
            BlTrace(
                "BetterLooting: SMART_LOOT DIRECT_THEN_RADIAL_SKIP transfer fault"
            );
        }
        else
        {
            Character* livePlayerCharacter =
                GetBetterLootingRadiusCenter(self);


            const bool radialStarted =
                livePlayerCharacter &&
                destinationWindow &&
                StartBetterLootingRadiusRun(
                    destinationWindow,
                    livePlayerCharacter,
                    excludedInventory
                );


            char chainMessage[256];

            sprintf_s(
                chainMessage,
                "BetterLooting: SMART_LOOT DIRECT_THEN_RADIAL_START started=%d",
                radialStarted ? 1 : 0
            );

            BlTrace(chainMessage);
        }
    }
}

static void LogClassification(
    const char* source,
    InventoryGUI* guiWindow,
    const hand& owner
)
{
    InventoryKind kind =
        ClassifyInventory(
            guiWindow,
            owner
        );

    Inventory* inventory =
        guiWindow
            ? guiWindow->getInventory()
            : NULL;

    char message[512];

    sprintf_s(
        message,
        "BetterLooting: CLASS source=%s kind=%s gui=%p inventory=%p items=%d",
        source,
        InventoryKindName(kind),
        guiWindow,
        inventory,
        inventory
            ? inventory->getNumItems()
            : -1
    );

    BlTrace(message);


    if (kind == InventoryKind::PLAYER_MAIN)
    {
        g_playerMainWindow = guiWindow;
    }
    else if (
        kind == InventoryKind::PLAYER_BACKPACK
    )
    {
        g_playerBackpackWindow = guiWindow;
    }


    if (
        kind == InventoryKind::CHARACTER_TARGET ||
        kind == InventoryKind::CONTAINER_TARGET
    )
    {
        g_lastLootTargetWindow =
            guiWindow;

        g_lastLootTargetInventory =
            guiWindow
                ? guiWindow->getInventory()
                : NULL;

        InventoryGUI* destination =
            SelectLootDestination();

        char routeMessage[256];

        sprintf_s(
            routeMessage,
            "BetterLooting: ROUTE source=%p destination=%p destinationKind=%s",
            guiWindow,
            destination,
            LootDestinationName(destination)
        );

        BlTrace(routeMessage);

        BlTrace(
            "BetterLooting: SMART_LOOT TARGET_CACHE_ONLY"
        );
    }
}

InventoryGUI* ShowInventory_hook(
    ForgottenGUI* self,
    const hand& owner,
    bool autoPosition,
    float x,
    float y
)
{

    /* Smart Loot uses INSERT state at target classification */
    InventoryGUI* result =
        g_origShowInventory(
            self,
            owner,
            autoPosition,
            x,
            y
        );

    LogClassification(
        "showInventory",
        result,
        owner
    );

    return result;
}


InventoryGUI* ShowInventoryNPC_hook(
    ForgottenGUI* self,
    const hand& owner
)
{

    /* Smart Loot uses INSERT state at target classification */
    InventoryGUI* result =
        g_origShowInventoryNPC(
            self,
            owner
        );

    LogClassification(
        "showInventoryNPC",
        result,
        owner
    );

    return result;
}


InventoryGUI* ShowInventoryBuilding_hook(
    ForgottenGUI* self,
    const hand& owner
)
{

    /* Smart Loot uses INSERT state at target classification */
    InventoryGUI* result =
        g_origShowInventoryBuilding(
            self,
            owner
        );

    LogClassification(
        "showInventoryBuilding",
        result,
        owner
    );

    return result;
}


InventoryGUI* ShowTraderInventory_hook(
    ForgottenGUI* self,
    const hand& owner
)
{

    /* Smart Loot uses INSERT state at target classification */
    InventoryGUI* result =
        g_origShowTraderInventory(
            self,
            owner
        );

    LogClassification(
        "showTraderInventory",
        result,
        owner
    );

    return result;
}





// ============================================================
// BL_MYGUI_PANEL
//
// Окно настроек живёт в BetterLootingPanel.cpp и построено на родном
// интерфейсе игры. Здесь остались только две точки, которыми панель
// дотягивается до логики мода, и запись настроек в файл.
// ============================================================

extern "C" IMAGE_DOS_HEADER __ImageBase;


void BetterLootingRequestStoreAll()
{
    const LONG previous =
        InterlockedExchange(
            &g_storeAllRequested,
            1
        );

    BlTrace(
        previous == 0
            ? "BetterLooting: PANEL store-all queued"
            : "BetterLooting: PANEL store-all already queued"
    );
}


static std::string BetterLootingIniPath()
{
    char modulePath[MAX_PATH] = {};

    const DWORD length =
        GetModuleFileNameA(
            reinterpret_cast<HMODULE>(
                &__ImageBase
            ),
            modulePath,
            MAX_PATH
        );

    if (
        length == 0 ||
        length >= MAX_PATH
    )
    {
        return "BetterLooting.ini";
    }

    std::string path(
        modulePath,
        length
    );

    const std::string::size_type separator =
        path.find_last_of("\\/");

    if (separator == std::string::npos)
    {
        return "BetterLooting.ini";
    }

    return
        path.substr(
            0,
            separator + 1
        ) +
        "BetterLooting.ini";
}


static void BetterLootingWriteBool(
    const std::string& path,
    const char* section,
    const char* key,
    bool value
)
{
    WritePrivateProfileStringA(
        section,
        key,
        value ? "1" : "0",
        path.c_str()
    );
}


void BetterLootingSaveRules()
{
    const std::string path =
        BetterLootingIniPath();

    char buffer[64] = {};

    sprintf_s(
        buffer,
        "%.2f",
        g_betterLootingRules.lootRadius
    );

    WritePrivateProfileStringA(
        "General",
        "LootRadius",
        buffer,
        path.c_str()
    );

    sprintf_s(
        buffer,
        "%d",
        g_betterLootingRules.minValue
    );

    WritePrivateProfileStringA(
        "Rules",
        "MinValue",
        buffer,
        path.c_str()
    );

    sprintf_s(
        buffer,
        "%.2f",
        g_betterLootingRules.minValuePerKg
    );

    WritePrivateProfileStringA(
        "Rules",
        "MinValuePerKg",
        buffer,
        path.c_str()
    );

    BetterLootingWriteBool(path, "Rules", "AllowStolen", g_betterLootingRules.allowStolen);

    BetterLootingWriteBool(path, "Categories", "Weapons", g_betterLootingRules.lootWeapons);
    BetterLootingWriteBool(path, "Categories", "Armour", g_betterLootingRules.lootArmour);
    BetterLootingWriteBool(path, "Categories", "Materials", g_betterLootingRules.lootMaterials);
    BetterLootingWriteBool(path, "Categories", "Food", g_betterLootingRules.lootFood);
    BetterLootingWriteBool(path, "Categories", "Medicine", g_betterLootingRules.lootMedicine);
    BetterLootingWriteBool(path, "Categories", "Tools", g_betterLootingRules.lootTools);
    BetterLootingWriteBool(path, "Categories", "Ammo", g_betterLootingRules.lootAmmo);
    BetterLootingWriteBool(path, "Categories", "Blueprints", g_betterLootingRules.lootBlueprints);
    BetterLootingWriteBool(path, "Categories", "Books", g_betterLootingRules.lootBooks);
    BetterLootingWriteBool(path, "Categories", "Robotics", g_betterLootingRules.lootRobotics);
    BetterLootingWriteBool(path, "Categories", "Narcotics", g_betterLootingRules.lootNarcotics);
    BetterLootingWriteBool(path, "Categories", "SeveredLimbs", g_betterLootingRules.lootSeveredLimbs);
    BetterLootingWriteBool(path, "Categories", "Other", g_betterLootingRules.lootOther);

    BetterLootingWriteBool(path, "Behavior", "BestItemsFirst", g_betterLootingRules.bestItemsFirst);
    BetterLootingWriteBool(path, "Behavior", "PreferBackpack", g_betterLootingRules.preferBackpack);

    sprintf_s(
        buffer,
        "%d",
        g_betterLootingRules.activationVirtualKey
    );

    WritePrivateProfileStringA(
        "Behavior",
        "ActivationVirtualKey",
        buffer,
        path.c_str()
    );

    BlTrace(
        "BetterLooting: PANEL settings saved"
    );
}


// ============================================================
// Страница в ModConfigMenu (вкладка MCM окна «Настройки»). Без MCM мод
// работает как раньше: настройки в BetterLooting.ini и в своей панели.
// MCM пишет ini и зовёт McmReload - правила перечитываются на ходу.
// ============================================================
#define KLOC_DOMAIN "better_looting"
#include <Localization.h>
#include <ModConfigMenu.h>

static void __cdecl McmReload()
{
    LoadBetterLootingRules();
}

// Кнопки пресетов. После нажатия MCM перечитывает страницу из ini.
static int __cdecl McmPreset(void* ud, char*, unsigned)
{
    BetterLootingApplyPreset(static_cast<int>(reinterpret_cast<intptr_t>(ud)));
    return 0;
}

extern "C" __declspec(dllexport) void MCM_Describe(MCM_Api* api)
{
    const std::string ini = BetterLootingIniPath();
    api->beginMod(api, "BetterLooting", "Better Looting", ini.c_str(), &McmReload);
    if (api->version >= 2)
        api->info(api, Tr("Collects loot around the selected character with one key and puts it away by your rules."));

    static const char* const keyValues[] = {
        "45", "36", "35", "46", "33", "34",
        "112", "113", "114", "115", "116", "117", "118", "119", "120", "121", "122", "123" };
    static const char* const keyLabels[] = {
        "Insert", "Home", "End", "Delete", "PageUp", "PageDown",
        "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12" };

    if (api->version >= 3)
    {
        api->section(api, Tr("Profiles"));
        api->action(api, Tr("Profile: balanced"),
                    Tr("Radius 20 m, from 500 and from 300 per kg; everything except narcotics and severed limbs"),
                    &McmPreset, reinterpret_cast<void*>(0), 0);
        api->action(api, Tr("Profile: money"),
                    Tr("Radius 25 m, from 1000 and from 500 per kg; weapons, armour, blueprints, books, robotics, narcotics and the rest"),
                    &McmPreset, reinterpret_cast<void*>(1), 0);
        api->action(api, Tr("Profile: resources"),
                    Tr("Radius 30 m, any price; materials, food, medicine, tools, ammo and robotics"),
                    &McmPreset, reinterpret_cast<void*>(2), 0);
        api->action(api, Tr("All categories"), Tr("Turn every category below on"),
                    &McmPreset, reinterpret_cast<void*>(3), 0);
        api->action(api, Tr("No categories"), Tr("Turn every category below off"),
                    &McmPreset, reinterpret_cast<void*>(4), 0);
    }

    api->section(api, Tr("Looting"));
    api->choice(api, "Behavior", "ActivationVirtualKey", Tr("Looting key"),
                Tr("Key that collects loot around the selected character."),
                "45", keyValues, keyLabels, 18, 0);
    api->number(api, "General", "LootRadius", Tr("Loot radius (m)"),
                Tr("How far around the character the mod looks for loot."),
                20.0f, 1.0f, 100.0f, 1, 0);
    api->toggle(api, "Behavior", "BestItemsFirst", Tr("Best items first"),
                Tr("Sort by value, so a full backpack holds the best finds."), 1, 0);
    api->toggle(api, "Behavior", "PreferBackpack", Tr("Prefer backpack"),
                Tr("Fill the backpack before the character's own inventory."), 1, 0);

    api->section(api, Tr("Rules"));
    api->integer(api, "Rules", "MinValue", Tr("Minimum value"),
                 Tr("Items cheaper than this are left where they are."), 0, 0, 5000, 0);
    api->number(api, "Rules", "MinValuePerKg", Tr("Minimum value per kg"),
                Tr("Cuts off heavy items with a poor value to weight ratio."),
                0.0f, 0.0f, 2000.0f, 1, 0);
    api->toggle(api, "Rules", "AllowStolen", Tr("Allow stolen items"),
                Tr("Pick up goods that still belong to someone else."), 1, 0);

    struct Category { const char* key; const char* label; int def; };
    static const Category categories[] = {
        { "Weapons", "Weapons", 1 }, { "Armour", "Armour", 1 },
        { "Materials", "Materials and resources", 1 }, { "Food", "Food", 1 },
        { "Medicine", "Medicine", 1 }, { "Tools", "Tools", 1 }, { "Ammo", "Ammo", 1 },
        { "Blueprints", "Blueprints", 1 }, { "Books", "Books", 1 },
        { "Robotics", "Robotics", 1 }, { "Narcotics", "Narcotics", 1 },
        { "SeveredLimbs", "Severed limbs", 1 }, { "Other", "Other", 1 } };
    api->section(api, Tr("Categories"));
    for (size_t i = 0; i < sizeof(categories) / sizeof(categories[0]); ++i)
        api->toggle(api, "Categories", categories[i].key, Tr(categories[i].label),
                    Tr("Collect items of this kind"), categories[i].def, 0);

    api->section(api, Tr("Advanced"));
    api->toggle(api, "Debug", "Log", Tr("Debug logging"),
                Tr("Write every looting step to RE_Kenshi_log.txt. Failures are logged anyway"), 0, 0);
}

__declspec(dllexport) void startPlugin()
{
    BlTrace(
        "BetterLooting: startPlugin called"
    );

    LoadBetterLootingRules();

    if (
        KenshiLib::SUCCESS !=
        KenshiLib::AddHook(
            KenshiLib::GetRealAddress(
                &Inventory::getExcessLoot
            ),
            InventoryGetExcessLoot_hook,
            &g_origInventoryGetExcessLoot
        )
    )
    {
        BlTrace(
            "BetterLooting: AUTODEPOSIT_HOOK_FAILED getExcessLoot"
        );
    }
    else
    {
        BlTrace(
            "BetterLooting: selective auto-deposit protection hook installed"
        );
    }


    KenshiLib::AddHook(
        KenshiLib::GetRealAddress(
            &InventoryGUI::_NV_update
        ),
        InventoryGUIUpdate_hook,
        &g_origInventoryGUIUpdate
    );

    KenshiLib::AddHook(
        KenshiLib::GetRealAddress(
            &ForgottenGUI::showInventory
        ),
        ShowInventory_hook,
        &g_origShowInventory
    );

    KenshiLib::AddHook(
        KenshiLib::GetRealAddress(
            &ForgottenGUI::showInventoryNPC
        ),
        ShowInventoryNPC_hook,
        &g_origShowInventoryNPC
    );

    KenshiLib::AddHook(
        KenshiLib::GetRealAddress(
            &ForgottenGUI::showInventoryBuilding
        ),
        ShowInventoryBuilding_hook,
        &g_origShowInventoryBuilding
    );

    KenshiLib::AddHook(
        KenshiLib::GetRealAddress(
            &ForgottenGUI::showTraderInventory
        ),
        ShowTraderInventory_hook,
        &g_origShowTraderInventory
    );

    BlTrace(
        "BetterLooting: classifier hooks installed"
    );

    // Окна настроек и SHIFT+L больше нет: настройки - во вкладке MCM.
}
