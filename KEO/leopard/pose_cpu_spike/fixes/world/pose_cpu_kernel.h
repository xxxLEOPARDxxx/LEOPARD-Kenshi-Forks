#pragma once
#include <ogre/OgrePose.h>
#include <ogre/OgreVector3.h>
namespace pose_cpu_kernel {
typedef Ogre::Pose::VertexOffsetMap Map;
struct Work {
 float weight;
 const Map*positions;
 const Map*normals;
 unsigned char*bytes;
 size_t stride,vertices,normalOffset;
 bool useNormals;
};
// Validated CPU-owned output only. No allocation, logging or engine API calls.
void Run(const Work&work,size_t begin,size_t end);
}
