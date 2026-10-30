#pragma once
struct StackSortSettings;
namespace StackSortMcm
{
void LoadStartup(StackSortSettings& cfg);
bool Enabled();
bool MergeStacks();
int Solutions();
}
