#pragma once
#include <string>

namespace BetterLootingPresets
{
    enum { SlotCount = 5 };
    enum Operation { Apply, Save, Clear };
    enum Result { Ok, InvalidSlot, EmptySlot, ValueTooLong, IoError };

    struct Slot
    {
        bool occupied;
        std::string name;
    };

    struct State
    {
        Slot slots[SlotCount];
        int active;
        bool modified;
    };

    State ReadState(const std::string& path);
    Result Execute(const std::string& path, int slot, Operation operation, const std::string& name);
}
