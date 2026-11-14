#include "resources/resource_biome_manifest.h"
#include <cstdio>
#include <limits>
using namespace resource_biome;
static int failures=0,checks=0;
#define CHECK(x) do{++checks;if(!(x)){++failures;std::printf("FAIL line %d: %s\n",__LINE__,#x);}}while(0)
int main(){
    CellList c;Bounds b(-147456,-147456,-142848,-142848);
    CHECK(CellsForBounds(b,c)==Ok&&c.count==1&&c.index[0]==0);
    b=Bounds(-140000,-140000,-135392,-135392);
    CHECK(CellsForBounds(b,c)==Ok&&c.count==4);
    CHECK(c.index[0]==0&&c.index[1]==1&&c.index[2]==32&&c.index[3]==33);
    b=Bounds(142848,142848,147456,147456);
    CHECK(CellsForBounds(b,c)==Ok&&c.count==1&&c.index[0]==1023);
    CHECK(CellsForBounds(Bounds(147456,0,147457,1),c)==BadBounds&&c.count==0);
    CHECK(CellsForBounds(Bounds(-147457,0,-147456,1),c)==BadBounds);
    CHECK(CellsForBounds(Bounds(1,0,0,1),c)==BadBounds);
    CHECK(CellsForBounds(Bounds(0,0,0,1),c)==BadBounds);
    CHECK(CellsForBounds(Bounds(0,0,20000,1),c)==TooManyCells);
    CHECK(CellsForBounds(Bounds(std::numeric_limits<double>::quiet_NaN(),0,1,1),c)==BadBounds);
    CHECK(CellsForBounds(Bounds(0,0,std::numeric_limits<double>::infinity(),1),c)==BadBounds);
    for(unsigned mask=0;mask<64;++mask)for(unsigned role=0;role<6;++role){
        int actual=RoleSource(role,mask);
        int expected=(mask&(1u<<role))?int(role):(mask&1)?0:-1;
        CHECK(actual==expected);
    }
    CHECK(RoleSource(6,63)==-1);
    CHECK(RoleSource(100,63)==-1);
    std::printf("resource biome pure: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
