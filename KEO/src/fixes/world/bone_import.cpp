#include "fixes/world/bone_import.h"
#include <stdint.h>
#include <cstddef>
#include <cstring>
namespace bone_import {
template<class T>static T Read(const void*p,size_t offset){T value;std::memcpy(&value,(const char*)p+offset,sizeof(value));return value;}
struct Pair{uintptr_t key;unsigned char record[12];};
static_assert(sizeof(Pair)==24&&offsetof(Pair,record)==8,"native assignment pair ABI");
void Add(void*self,const void*assignment,Original original,Allocate allocate,Link link,bool enabled,bool owner){
 if(!owner||!enabled||!self||!assignment||!allocate||!link||Read<unsigned char>(self,0)){original(self,assignment);return;}
 // Match the original's integer copies BEFORE allocation. The input can point
 // into the same tree, and allocation may invoke a reentrant new_handler.
 Pair pair;pair.key=Read<unsigned>(assignment,0);
 void*map=(char*)self+0xd0;
 // A cheap admission check keeps ordinary unsorted imports on the original
 // path. None of these pre-allocation pointers survives the allocation.
 {void*head=Read<void*>(map,8);if(Read<void*>(head,8)!=head&&pair.key<Read<uintptr_t>(Read<void*>(head,0x10),0x18)){original(self,assignment);return;}}
 std::memcpy(pair.record,assignment,12);
 void*node=allocate(map,&pair);
 // After this point the single native node must reach native Link exactly
 // once. No original-call fallback, allocation, logging or throwing callback.
 void*head=Read<void*>(map,8);void*parent=head;void*cursor=Read<void*>(head,8);bool left=true;
 const uintptr_t key=Read<uintptr_t>(node,0x18);
 if(cursor!=head){
  void*last=Read<void*>(head,0x10);
  if(key>=Read<uintptr_t>(last,0x18)){parent=last;left=false;}
  else{
   do{parent=cursor;left=key<Read<uintptr_t>(cursor,0x18);cursor=Read<void*>(cursor,left?0:0x10);}while(!Read<unsigned char>(cursor,0x31));
  }
 }
 void*out=NULL;link(map,&out,left,parent,node);
 // Native Link owns max-size cleanup and can throw. Dirty is written only
 // after it returns, exactly as in SubMesh::addBoneAssignment.
 *((unsigned char*)self+0xf8)=1;
}
}
