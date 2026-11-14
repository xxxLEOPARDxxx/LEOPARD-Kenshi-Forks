#pragma once
#include <vector>
#include <string>
namespace resource_gpu {
struct DdsMip {size_t offset, bytes;unsigned width,height,rowPitch;};
struct Dds {unsigned width,height,levels,format;std::vector<DdsMip> mips;Dds():width(0),height(0),levels(0),format(0){}};
// Native terrain DDS subset only. All source levels must exist, even levels
// discarded by native additional-mips-minus-three/compressed-minus-two rules.
bool ParseDds(const std::vector<unsigned char>& bytes,unsigned type,Dds* out,std::string* error);
}
