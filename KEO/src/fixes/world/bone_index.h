#pragma once
#include <cstddef>
namespace bone_index {
typedef void(*Grow)(void*,size_t);
// Refusal is read-only. Once growth starts, exceptions propagate directly.
bool Try(const void*assignments,void*boneToBlend,void*blendToBone,Grow);
}
void InstallBoneIndex(int*,int*);
