#pragma once
#include "fixes/world/pose_cpu_kernel.h"
namespace Ogre{class VertexData;}
namespace pose_cpu_blend {
void Init();
bool Candidate(float weight,const void*positions,const void*normals);
// Caller separately proves current private morph scope and exact native site.
bool Try(float weight,const pose_cpu_kernel::Map&positions,const pose_cpu_kernel::Map&normals,Ogre::VertexData*target);
void SignalStop();
void Stop();
}
