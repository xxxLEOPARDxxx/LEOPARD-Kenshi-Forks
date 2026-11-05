#pragma once
#include <stdint.h>
namespace keo_inventory {
struct WeightBody {float x,y,z,scale,height,frame,hunger;int robot;};
// Native callbacks own no pointers beyond this synchronous query. child() returns
// only an unequipped container's inventory; nativeWeight preserves game reductions.
struct WeightOps {
 void*ctx;float(*nativeWeight)(void*,uintptr_t);uintptr_t(*owner)(void*,uintptr_t);
 int(*count)(void*,uintptr_t);uintptr_t(*child)(void*,uintptr_t,int);
 uintptr_t(*carried)(void*,uintptr_t);uintptr_t(*inventory)(void*,uintptr_t);
 bool(*body)(void*,uintptr_t,WeightBody*);bool(*nativeBody)(void*,uintptr_t,uintptr_t,float*);
};
float CarriedBodyFactor(const WeightBody& body);
float CarriedWeightEvaluate(const WeightOps& ops,uintptr_t inventory,float nativeWeight);
}
