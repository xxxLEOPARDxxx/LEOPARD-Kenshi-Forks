#pragma once
#include <string>
#include "resources/resource_gpu_store.h"
void ResourceForwardInit();
void ResourceForwardTick(void* zoneManager,const void* cameraPosition,bool loading);
void ResourceForwardReset(); // signal only, main tick owns the reset
bool ResourceBuildTerrainJob(const void* biome,int type,resource_gpu::Job&);
unsigned __int64 ResourceWorldGeneration();
void ResourceGpuBridgeInit();
bool ResourceGpuBridgeTick(bool enabled);
void ResourceGpuBridgeStop();
std::string ResourceGpuBridgeStats();
