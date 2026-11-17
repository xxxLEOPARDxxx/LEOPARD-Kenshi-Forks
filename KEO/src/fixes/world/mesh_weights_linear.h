#pragma once
#include <ogre/OgreMesh.h>
#include <xmmintrin.h>
#include <cstring>
namespace mesh_weights {
// First pass is integer-only and never mutates a native node. Rejection can
// therefore call the original exactly once without repeating normalization.
inline bool Try(size_t vertices,Ogre::Mesh::VertexBoneAssignmentList&list,unsigned short*out){
 if(!out||!vertices||vertices>1048576||list.size()<vertices||list.size()>vertices*4||(_mm_getcsr()&0x1f80)!=0x1f80)return false;
 typedef Ogre::Mesh::VertexBoneAssignmentList List;List::iterator it=list.begin(),end=list.end();unsigned short maximum=0;
 for(size_t vertex=0;vertex<vertices;++vertex){
  unsigned count=0,nonzero=0;
  while(it!=end&&it->first==vertex){
   if(++count>4||it->second.vertexIndex!=vertex)return false;
   unsigned bits;std::memcpy(&bits,&it->second.weight,4);
   // Ordinary finite nonnegative [0,1] weights, including +0. Unusual assets
   // and denormals retain native handling and native diagnostics.
   if(bits>0x3f800000u||(bits&&bits<0x00800000u))return false;
   nonzero|=bits;++it;
  }
  if(!count||!nonzero)return false;
  if(count>maximum)maximum=(unsigned short)count;
 }
 if(it!=end)return false;
 it=list.begin();
 for(size_t vertex=0;vertex<vertices;++vertex){
  float*weights[4];unsigned count=0;float total=0.0f;
  do{weights[count++]=&it->second.weight;total+=it->second.weight;++it;}while(it!=end&&it->first==vertex);
  // Exact installed Ogre SSE sequence: ordered scalar sum, fabs(1-sum),
  // FLT_EPSILON threshold, one reciprocal, then reciprocal*each weight.
  float distance=1.0f-total;unsigned bits;std::memcpy(&bits,&distance,4);bits&=0x7fffffffu;std::memcpy(&distance,&bits,4);
  if(distance>1.1920928955078125e-7f){const float factor=1.0f/total;for(unsigned j=0;j<count;++j)*weights[j]=factor**weights[j];}
 }
 *out=maximum;return true;
}
}
