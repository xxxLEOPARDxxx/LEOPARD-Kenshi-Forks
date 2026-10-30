#pragma once
namespace SortPolicy
{
inline bool SortableGrid(int width, int height, int limit, bool equipped, bool enabled)
{
    return enabled && !equipped && width > 0 && height > 0 &&
        (width > 1 || height > 1) && limit != 1;
}
inline bool CanPublish(long aborted, long generation, long expected, int slot, int slots)
{
    return aborted == 0 && generation == expected && slot >= 1 && slot <= slots;
}
}
