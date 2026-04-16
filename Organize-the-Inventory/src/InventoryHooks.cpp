#include "InventoryHooks.h"

#include "InventoryCore.h"
#include "InventorySearchUi.h"

#include <core/Functions.h>
#include <kenshi/PlayerInterface.h>

#include <cstdint>
#include <sstream>

namespace
{
typedef void (*PlayerInterfaceUpdateUTFn)(PlayerInterface*);

PlayerInterfaceUpdateUTFn g_updateUTOrig = 0;

void PlayerInterface_updateUT_hook(PlayerInterface* self)
{
    if (g_updateUTOrig != 0)
    {
        g_updateUTOrig(self);
    }

    TickInventorySearchUi();
}
}

bool InstallInventoryHooks()
{
    const std::uintptr_t updateAddress = KenshiLib::GetRealAddress(&PlayerInterface::updateUT);
    if (updateAddress == 0
        || KenshiLib::SUCCESS != KenshiLib::AddHook(
            updateAddress,
            PlayerInterface_updateUT_hook,
            &g_updateUTOrig))
    {
        return false;
    }

    std::stringstream line;
    line << "hooked PlayerInterface::updateUT";
    LogInfoLine(line.str());
    return true;
}
