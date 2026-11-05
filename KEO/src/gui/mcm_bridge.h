#pragma once
#include "gui/ModConfigMenu.h"
#include "gui/settings_rows.h"

// Main/UI thread. The optional MCM client never owns native/world pointers.
void KeoMcmDescribe(MCM_Api* api);
void KeoMcmSetPlatformReady(bool ready); // native translation permitted by build gate
void KeoMcmSetRuntimeReady(bool ready);
bool KeoMcmRuntimeReady();
bool KeoMcmAvailable(); // probe the optional v4+ export at Options-create time
// Merge only edits relative to baseline into the current requested snapshot.
// Save atomically before publication/live apply. Nonzero reports failure.
int KeoMcmCommitStaging(const SettingsStaging& edited, const SettingsStaging& baseline,
                        char* error, unsigned errorSize);
// Native boundary, invoked only after the runtime gate permits it.
ConfigFloor KeoMcmRuntimeFloor();
