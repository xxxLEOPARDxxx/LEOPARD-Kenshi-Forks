#include <cstdlib>
#include <new>
static int allocationFailAfter=-1;
static unsigned allocationFailures=0;
static void(*allocationFailureProbe)()=NULL;
static size_t watchedAllocationSize=0;
static unsigned watchedAllocations=0;
void*operator new(size_t n){
    if(watchedAllocationSize&&n==watchedAllocationSize)++watchedAllocations;
    if(allocationFailAfter>=0&&allocationFailAfter--==0){allocationFailAfter=-1;++allocationFailures;if(allocationFailureProbe)allocationFailureProbe();throw std::bad_alloc();}
    void*p=malloc(n?n:1);if(!p)throw std::bad_alloc();return p;
}
void operator delete(void*p){free(p);}
