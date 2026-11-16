#include "equipment_field_lookup.h"
#include <cstring>
namespace LeopardEquipmentField {
namespace {
struct SlotHash {size_t operator()(const std::string&)const{return 0x28253c9a37862ULL;}};
}
IntPair*__fastcall Slot(IntMap*map,const std::string*key,Original original,bool enabled,bool onOwnerThread){
 if(enabled&&onOwnerThread&&map&&key&&key->size()==4&&!std::memcmp(key->data(),"slot",4)){
  IntMap::iterator found=map->find(*key,SlotHash(),std::equal_to<std::string>());
  if(found!=map->end())return &*found;
 }
 // Only a real hit bypasses the original. Missing fields retain its insertion,
 // default construction, rehash, allocation failures and exception behavior.
 return original(map,key);
}
}
