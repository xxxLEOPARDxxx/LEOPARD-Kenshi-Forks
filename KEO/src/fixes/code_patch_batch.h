#pragma once
#include "fixes/code_patch_threads.h"
namespace code_patch_batch {
struct Edit{unsigned char*address;unsigned size;unsigned char before[8],after[8];};
// Startup only, at most16 disjoint edits. No heap/logging while excluded.
// Owner keeps any branch island when mayPublish is true, even after failure.
bool Apply(const Edit*,unsigned,code_patch_threads::Set*,bool*mayPublish);
}
