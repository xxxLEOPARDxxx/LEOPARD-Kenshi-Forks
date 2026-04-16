#pragma once

#include "InventoryCore.h"

void NormalizeInventoryConfigSnapshot(InventoryConfigSnapshot* config);
InventoryConfigSnapshot CaptureInventoryConfigSnapshot();
void ApplyInventoryConfigSnapshot(const InventoryConfigSnapshot& config);
bool SaveInventoryConfigSnapshot(const InventoryConfigSnapshot& config);
void LoadInventoryConfig();
