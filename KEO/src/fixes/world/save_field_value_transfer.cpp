#include "fixes/world/save_field_value_transfer.h"
#include <stdint.h>
namespace save_field_value_transfer {
static_assert(_ITERATOR_DEBUG_LEVEL==0&&sizeof(std::string)==40,"native VC10 release string ABI");
static bool Disjoint(uintptr_t a,size_t na,uintptr_t b,size_t nb){
 return a<=~(uintptr_t)0-na&&b<=~(uintptr_t)0-nb&&(a+na<=b||b+nb<=a);
}
bool Try(std::string*dst,std::string*src){
 if(!dst||!src||!Disjoint((uintptr_t)dst,sizeof(*dst),(uintptr_t)src,sizeof(*src)))return false;
 const size_t size=src->size(),capacity=src->capacity();
 if(size<=15||size>1048576||capacity<size||capacity>(size|15)||!dst->empty()||dst->capacity()!=15)return false;
 const uintptr_t data=(uintptr_t)src->data();
 if(!data||!Disjoint(data,capacity+1,(uintptr_t)src,sizeof(*src))||!Disjoint(data,capacity+1,(uintptr_t)dst,sizeof(*dst)))return false;
 // No allocation, callback or destruction here. Native later destroys both
 // strings; the old local becomes empty SSO and the field owns its same buffer.
 dst->swap(*src);return true;
}
}
