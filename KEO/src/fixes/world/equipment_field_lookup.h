#pragma once
#include <kenshi/util/OgreUnordered.h>
#include <string>
namespace LeopardEquipmentField {
typedef ogre_unordered_map<std::string,int>::type IntMap;
typedef IntMap::value_type IntPair;
typedef IntPair*(__fastcall*Original)(IntMap*,const std::string*);
// Pair-address ABI, not mapped-value address. Exact caller/thread/patch gates
// belong to the caller; this function owns no native pointers or metadata.
IntPair*__fastcall Slot(IntMap*,const std::string*,Original,bool enabled,bool onOwnerThread);
}
