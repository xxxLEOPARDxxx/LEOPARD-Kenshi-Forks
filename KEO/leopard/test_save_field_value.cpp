#include <string>
#include <stdint.h>
#include <cstdio>
#include <cstdlib>
#include <new>
static bool track=false,fail=false;static unsigned allocations=0,frees=0,checks=0;
static const void*watched=NULL;static unsigned watchedFrees=0;
void*operator new(size_t n){if(track){++allocations;if(fail)throw std::bad_alloc();}void*p=malloc(n?n:1);if(!p)throw std::bad_alloc();return p;}
void operator delete(void*p){if(p&&p==watched)++watchedFrees;if(track&&p)++frees;free(p);}
#ifdef KEO_FIELD_VALUE_BASELINE
namespace save_field_value_transfer{bool Try(std::string*,std::string*){return false;}}
#else
#include "fixes/world/save_field_value_transfer.h"
#endif
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL %u line %u: %s\n",checks,__LINE__,#x);exit(1);}}while(0)
int main(){
 for(size_t n=16;n<512;++n){std::string src(n,'v'),dst;const char*p=src.data();allocations=frees=0;track=fail=true;bool moved=save_field_value_transfer::Try(&dst,&src);track=fail=false;CHECK(moved);CHECK(dst.data()==p&&dst.size()==n&&dst[0]=='v'&&dst[n-1]=='v');CHECK(src.empty()&&src.capacity()==15&&allocations==0&&frees==0);}
 {std::string src(15,'a'),dst;CHECK(!save_field_value_transfer::Try(&dst,&src)&&src.size()==15&&dst.empty());}
 {std::string src(16,'a'),dst("old");CHECK(!save_field_value_transfer::Try(&dst,&src)&&dst=="old");}
 {std::string src(16,'a'),dst;dst.reserve(200);CHECK(!save_field_value_transfer::Try(&dst,&src)&&src.size()==16);}
 {std::string src(16,'a'),dst;src.reserve(500);CHECK(!save_field_value_transfer::Try(&dst,&src));CHECK(!save_field_value_transfer::Try(&src,&src));CHECK(!save_field_value_transfer::Try(NULL,&src));CHECK(!save_field_value_transfer::Try(&dst,NULL));}
 {std::string src(47,'a'),dst;CHECK(!save_field_value_transfer::Try((std::string*)((uintptr_t)&src+1),&src));CHECK(!save_field_value_transfer::Try((std::string*)~(uintptr_t)15,&src));CHECK(!save_field_value_transfer::Try(&dst,(std::string*)~(uintptr_t)15));}
 {std::string src(1048577,'a'),dst;CHECK(!save_field_value_transfer::Try(&dst,&src));}
 {std::string src(47,'a'),dst;src[2]=0;CHECK(save_field_value_transfer::Try(&dst,&src)&&dst.size()==47&&dst[2]==0&&dst[46]=='a');}
 {watchedFrees=0;std::string dst;{std::string src(47,'z');watched=src.data();CHECK(save_field_value_transfer::Try(&dst,&src));}CHECK(watchedFrees==0&&dst.size()==47);}CHECK(watchedFrees==1);watched=NULL;
 printf("PASS field value: %u checks\n",checks);return 0;
}
