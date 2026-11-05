#include "game/native_profile.h"
#include <Windows.h>
#include <stdio.h>
int main(){unsigned char a[2]={1,2};uintptr_t base=(uintptr_t)GetModuleHandleW(NULL);
 if(NativeProfileIsGog()||NativeProfileAddress(base,0x123)!=base+0x123||NativeProfileCanonical(0x123)!=0x123||NativeProfileBytes(3,a,2)!=a||!NativeProfileMatches(3,a,a,2))return 1;
 if(NativeProfilePrepareGog(base,NULL)||NativeProfileIsGog())return 2;
 if(NativeProfileAddress(base,0x123)||NativeProfileCanonical(0x123)||NativeProfileBytes(3,a,2)||NativeProfileMatches(3,a,a,2))return 3;
 if(NativeProfilePrepareGog(0,NULL))return 4;
 puts("PASS native profile: Steam unchanged, wrong PE/SHA refused, every GOG refusal stays closed");return 0;}
