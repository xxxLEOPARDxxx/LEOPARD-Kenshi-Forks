#ifndef KEO_MORPH_POSE_SCOPE_H
#define KEO_MORPH_POSE_SCOPE_H
#if defined(_MSC_VER) && _MSC_VER < 1700
#include <stdint.h>
#else
#include <cstdint>
#endif
namespace morph_pose_scope {
typedef void*(*CreateFn)(const void*,void*,void*,bool);
typedef void*(*CloneFn)(void*,void*,const void*,const void*);
void* Create(CreateFn,const void*name,void*entity,void*data,bool shaved);
void* Clone(CloneFn,void*self,void*out,const void*name,const void*group,uintptr_t caller);
void CaptureCloneResult(void*result,void*out,uintptr_t caller);
void SetCloneHooksReady(bool);
void SetCreateHookReady(bool);
void Install(int*,int*);
}
#endif
