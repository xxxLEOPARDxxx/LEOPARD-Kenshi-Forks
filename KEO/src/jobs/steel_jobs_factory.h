#pragma once
#include <stdint.h>
#include <string>
namespace keo_jobs {
typedef void (*FactoryBody)(void*task,void*runner);
struct FactoryCallbacks {FactoryBody gather,waypoint,stock;};
// Main-thread startup/world-ready registration only. No UI/INI/worker ownership.
bool InitializeFactory(uintptr_t base,const FactoryCallbacks&,void(*log)(const char*));
// Early title-screen metadata registration. Native GOAP templates are not
// required here; saved custom task identities must exist before deserialization.
bool RegisterFactoryTask(int type,const std::string&caption);
// Install once, only after all three TaskData records are published.
bool InstallFactory();
// Complete the registered metadata once native GOAP templates exist. No frame
// polling: use a world-ready event. Create also ensures its own slot is ready.
bool PrepareFactoryTasks();
void* FactoryTaskData(int type);
FactoryBody FactoryNativeBody(int type);
uintptr_t FactoryCaptionAddress();
uintptr_t FactoryContextMenuAddress();
}
