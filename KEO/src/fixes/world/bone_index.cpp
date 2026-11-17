#include "fixes/world/bone_index.h"
#include <stdint.h>
#include <cstring>
#include <intrin.h>
#pragma intrinsic(_BitScanForward64)
namespace bone_index {
template<class T>static T Read(const void*p,size_t offset){T value;std::memcpy(&value,(const char*)p+offset,sizeof(value));return value;}
static void Size(void*p,size_t size){std::memcpy((char*)p+8,&size,sizeof(size));}
struct Array{uintptr_t data;size_t size,capacity;};
static_assert(sizeof(Array)==24,"native FastArray<uint16> layout");
static bool Valid(const Array&a){return a.size<=a.capacity&&a.capacity<=~size_t(0)/2&&(!a.capacity||a.data)&&a.data<=~uintptr_t(0)-a.capacity*2;}
static void Resize(void*array,size_t target,Grow grow){
 const size_t old=Read<size_t>(array,8);
 if(target>old){
  grow(array,target-old);
  // Native growth can invoke a new_handler. No old data/size borrow survives.
  const size_t begin=Read<size_t>(array,8);unsigned short*data=Read<unsigned short*>(array,0);
  for(size_t i=begin;i<target;++i)data[i]=0;
 }
 Size(array,target);
}
bool Try(const void*assignments,void*boneToBlend,void*blendToBone,Grow grow){
 if(!assignments||!boneToBlend||!blendToBone||boneToBlend==blendToBone||!grow)return false;
 const size_t count=Read<size_t>(assignments,0x10);
 // Tiny inputs keep the native path; scratch and traversal are bounded.
 if(count<128||count>1048576)return false;
 const Array a=Read<Array>(boneToBlend,0),b=Read<Array>(blendToBone,0);
 if(!Valid(a)||!Valid(b)||(a.capacity&&b.capacity&&a.data<b.data+b.capacity*2&&b.data<a.data+a.capacity*2))return false;
 uint64_t seen[1024]={0},usedWords[16]={0};unsigned unique=0,maximum=0;
 const void*head=Read<const void*>(assignments,8);const void*node=Read<const void*>(head,0);size_t visited=0;
 while(node!=head){
  if(visited++>=count)return false;
  const unsigned bone=Read<unsigned short>(node,0x24);const uint64_t bit=uint64_t(1)<<(bone&63);
  if(!(seen[bone>>6]&bit)){seen[bone>>6]|=bit;usedWords[bone>>12]|=uint64_t(1)<<((bone>>6)&63);++unique;if(bone>maximum)maximum=bone;}
  const void*right=Read<const void*>(node,0x10);
  if(!Read<unsigned char>(right,0x31)){node=right;while(!Read<unsigned char>(Read<const void*>(node,0),0x31))node=Read<const void*>(node,0);}
  else{const void*parent=Read<const void*>(node,8);while(!Read<unsigned char>(parent,0x31)&&node==Read<const void*>(parent,0x10)){node=parent;parent=Read<const void*>(parent,8);}node=parent;}
 }
 if(visited!=count)return false;
 // The temporary native set's allocations disappear intentionally. Output
 // allocation order, ownership and partial state on a throw remain native.
 Resize(blendToBone,unique,grow);Resize(boneToBlend,size_t(maximum)+1,grow);
 unsigned short*forward=Read<unsigned short*>(boneToBlend,0);unsigned short*inverse=Read<unsigned short*>(blendToBone,0);unsigned rank=0;
 // Visit only occupied words/bits: a sparse high index must not trigger a
 // scan over all65536 possible bones on every small model.
 for(unsigned group=0;group<16;++group){uint64_t words=usedWords[group];while(words){unsigned long word;_BitScanForward64(&word,words);const unsigned index=group*64+word;uint64_t bits=seen[index];while(bits){unsigned long bit;_BitScanForward64(&bit,bits);const unsigned bone=index*64+bit;forward[bone]=(unsigned short)rank;inverse[rank++]=(unsigned short)bone;bits&=bits-1;}words&=words-1;}}
 return true;
}
}
