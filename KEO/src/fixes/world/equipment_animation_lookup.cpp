#include "fixes/world/equipment_animation_lookup.h"
#include <stdint.h>
#include <cstring>
namespace equipment_animation_lookup {
template<class T>static T Read(const void*p,size_t offset){T value;memcpy(&value,(const char*)p+offset,sizeof(value));return value;}
struct Pair{unsigned key;unsigned padding;void*state;};
static_assert(sizeof(unsigned)==4&&sizeof(void*)==8&&sizeof(Pair)==16,"native animation table pair ABI");
void**Append(void*map,const unsigned*key,Original original,Allocate allocate,
             Insert insert,bool enabled,bool onOwnerThread){
 if(!onOwnerThread||!enabled||!map||!key||!allocate||!insert)return original(map,key);
 void*head=Read<void*>(map,8);
 if(!head||((uintptr_t)head&7)||!Read<unsigned char>(head,0x29))return original(map,key);
 void*last=Read<void*>(head,0x10);
 if(Read<uint64_t>(map,0x10)==0){
  if(last!=head)return original(map,key);
 }else{
  if(!last||last==head||((uintptr_t)last&7)||Read<unsigned char>(last,0x29)||*key<=Read<unsigned>(last,0x18))return original(map,key);
 }
 // Inspect CURRENT state after the outer AnimationState constructor, whose
 // string allocation can invoke a reentrant new_handler. Never reuse its
 // earlier lookup node. This pair matches native operator[]'s default value.
 const Pair pair={*key,0,NULL};
 void*node=allocate(map,&pair);
 // Allocation can itself reenter; native allocation also reads the head after
 // allocating. Pass the CURRENT end sentinel to native validated hint insertion.
 // From this point native insertion owns the node, including duplicate/size
 // failure cleanup. Do not add a post-allocation fallback or throwing callback.
 void*out=NULL;insert(map,&out,Read<void*>(map,8),node);
 return (void**)((char*)out+0x20);
}
}
