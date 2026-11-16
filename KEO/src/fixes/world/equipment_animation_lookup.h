#pragma once
namespace equipment_animation_lookup {
typedef void**(__fastcall*Original)(void*,const unsigned*);
typedef void*(__fastcall*Allocate)(void*,const void*);
typedef void*(__fastcall*Insert)(void*,void**,void*,void*);
// Only the authenticated native uint32->AnimationState* map under its existing
// createAnimationState lock. Original must be valid, including fallback paths.
void**Append(void* map,const unsigned* key,Original original,Allocate allocate,
             Insert insert,bool enabled,bool onOwnerThread);
}
