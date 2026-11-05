#pragma once
#include <stdint.h>
namespace keo_jobs {
uint64_t SteelJobsEpoch();
bool SteelJobsAdmitted();
void SteelJobsWorldHooksReady(bool ready);
void InstallSteelJobs(int* installed,int* total);
void SteelJobsMainTick(bool loading);
void SteelJobsWorldReset();
// Called before every existing KEO movement observer. True means this shift
// ground click belongs to the waypoint job and must not create a native move.
bool SteelJobsConsumeMove(void* player,void* building,void* subject,const float* location,bool shift);
}
