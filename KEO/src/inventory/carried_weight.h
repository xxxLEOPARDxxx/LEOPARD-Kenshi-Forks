#pragma once
namespace keo_inventory {
void InstallCarriedWeight(int* installed,int* total);
// Existing Main lifecycle callback; no engine actors retained. First post-startup
// check publishes admission; at most one exact module lookup per second later.
void CarriedWeightTick(double now);
}
