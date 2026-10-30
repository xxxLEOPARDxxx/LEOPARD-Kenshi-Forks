#pragma once

// Session-only memory of the target the user last picked, keyed by the object
// that owns the inventory plus which slot and dimension it was shown under.
// Nothing is written to disk: a relaunch starts empty.
//
// Main thread only. Every caller is a MyGUI delegate or hook_update, so there
// is no locking here.

struct TargetKey
{
    // Raw fields copied out of RootObjectBase::handle. Copied rather than
    // stored as a `hand` so this header pulls in no Kenshi types.
    unsigned int index;
    unsigned int serial;
    unsigned int container;
    unsigned int containerSerial;
    int type;

    int slot; // 0 = body or container, 1 = backpack
    int dim;  // Packer::TargetDim as an int, so H and W are remembered apart

    TargetKey() : index(0), serial(0), container(0), containerSerial(0), type(0), slot(0), dim(0) {}

    bool operator<(const TargetKey& o) const;
};

namespace TargetMemory
{
void Clear();
// target 0 forgets the key, which is what a right-click reset wants.
void Remember(const TargetKey& key, int target);

bool Recall(const TargetKey& key, int& outTarget);
} // namespace TargetMemory
