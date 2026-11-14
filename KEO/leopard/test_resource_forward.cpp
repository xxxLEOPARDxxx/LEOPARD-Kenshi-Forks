#include "resources/resource_forward_policy.h"
#include <cassert>
#include <iostream>
int main(){
    resource_forward::Cells c;
    resource_forward::Candidates(10,10,20,10,true,c);
    assert(c.count<=12 && c.count>0);
    assert(c.x[0]==12 && c.y[0]==10); // prioritize two cells in direction of travel
    bool adjacent=false;for(unsigned i=0;i<c.count;++i){assert(c.x[i]>=0&&c.x[i]<=63&&c.y[i]>=0&&c.y[i]<=63);if(c.x[i]==11&&c.y[i]==10)adjacent=true;for(unsigned j=0;j<i;++j)assert(c.x[i]!=c.x[j]||c.y[i]!=c.y[j]);}assert(adjacent);
    resource_forward::Candidates(0,0,-5,-5,true,c);for(unsigned i=0;i<c.count;++i)assert(c.x[i]>=0&&c.y[i]>=0);
    resource_forward::Candidates(-1,10,0,0,false,c);assert(!c.count);
    assert(!resource_forward::CanWarm(0.020,0.019,0.0,0.0));
    assert(!resource_forward::CanWarm(5.0,4.0,0.030,5.2));
    assert(resource_forward::CanWarm(6.0,5.9,0.005,5.2));
    std::string paths[6];for(unsigned i=0;i<6;++i)paths[i]="D:\\SteamLibrary\\steamapps\\common\\Kenshi\\data\\newland\\land\\textures\\long-terrain-name.dds";
    std::string key=resource_forward::TerrainKey(std::string(255,'s'),0,paths);assert(key.size()<512&&key.size()>256);
    std::string nativePaths[6];for(unsigned i=0;i<6;++i)nativePaths[i]="LONG-TERRAIN-NAME.DDS";
    assert(key==resource_forward::TerrainKey(std::string(255,'s'),0,nativePaths));
    paths[5]+="different";assert(key!=resource_forward::TerrainKey(std::string(255,'s'),0,paths));
    std::cout<<"forward resource policy passed\n";
}
