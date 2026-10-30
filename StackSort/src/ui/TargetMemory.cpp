#include <map>

#include "TargetMemory.h"

bool TargetKey::operator<(const TargetKey& o) const
{
    if (index != o.index) return index < o.index;
    if (serial != o.serial) return serial < o.serial;
    if (container != o.container) return container < o.container;
    if (containerSerial != o.containerSerial) return containerSerial < o.containerSerial;
    if (type != o.type) return type < o.type;
    if (slot != o.slot) return slot < o.slot;
    return dim < o.dim;
}

static std::map<TargetKey, int> s_memory;
void TargetMemory::Clear() { s_memory.clear(); }

void TargetMemory::Remember(const TargetKey& key, int target)
{
    if (target == 0)
    {
        s_memory.erase(key);
        return;
    }
    if (s_memory.size() >= 512 && s_memory.find(key) == s_memory.end()) s_memory.erase(s_memory.begin());
    s_memory[key] = target;
}

bool TargetMemory::Recall(const TargetKey& key, int& outTarget)
{
    std::map<TargetKey, int>::const_iterator it = s_memory.find(key);
    if (it == s_memory.end()) return false;
    outTarget = it->second;
    return true;
}
