#include "fixes/world/pose_cpu_kernel.h"
#include <xmmintrin.h>
namespace pose_cpu_kernel {
static void Apply(float weight,const Ogre::Vector3&value,unsigned char*destination){
 const float*source=&value.x;float*out=reinterpret_cast<float*>(destination);
 // Preserve native SSE operand order even for NaN payloads and signed zero.
 // Never touch an adjacent fourth attribute or padding lane.
 for(unsigned i=0;i<3;++i){const __m128 product=_mm_mul_ss(_mm_set_ss(weight),_mm_load_ss(source+i));_mm_store_ss(out+i,_mm_add_ss(product,_mm_load_ss(out+i)));}
}
static void Range(const Work&w,const Map&map,size_t begin,size_t end,size_t offset){
 Map::const_iterator it=map.lower_bound(begin);
 for(;it!=map.end()&&it->first<end;++it)Apply(w.weight,it->second,w.bytes+it->first*w.stride+offset);
}
void Run(const Work&w,size_t begin,size_t end){
 Range(w,*w.positions,begin,end,0);
 if(w.useNormals)Range(w,*w.normals,begin,end,w.normalOffset);
}
}
