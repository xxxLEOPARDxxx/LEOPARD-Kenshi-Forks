#include "resources/resource_dds.h"
#include <vector>
#include <cstdio>
#include <cstring>
#include <cstdlib>
static void Check(bool b,const char* msg){if(!b){std::printf("FAIL %s\n",msg);std::exit(1);}}
static void Put(std::vector<unsigned char>&v,size_t o,unsigned n){std::memcpy(&v[o],&n,4);}
static std::vector<unsigned char> Make(unsigned type){
    std::vector<unsigned char>v(128);Put(v,0,0x20534444);Put(v,4,124);Put(v,8,0xA1007);Put(v,12,2048);Put(v,16,2048);
    Put(v,28,12);Put(v,76,32);Put(v,80,4);Put(v,84,type?0x31545844:0x35545844);Put(v,108,0x401008);
    for(unsigned mip=0,side=2048;mip<12;++mip,side=side>1?side/2:1){unsigned blocks=(side+3)/4;v.resize(v.size()+blocks*blocks*(type?8:16),unsigned char(mip+1));}return v;
}
int main(){
    resource_gpu::Dds d;std::string error;
    for(unsigned t=0;t<2;++t){std::vector<unsigned char>v=Make(t);Check(resource_gpu::ParseDds(v,t,&d,&error),"valid DDS");Check(d.width==2048&&d.levels==7&&d.mips.size()==7,"native mip truncation");Check(d.mips[0].offset==128&&d.mips[0].rowPitch==512*(t?8:16),"base span");Check(d.mips[6].width==32&&d.mips[6].rowPitch==8*(t?8:16),"last native mip");Check(!resource_gpu::ParseDds(v,1-t,&d,&error),"type format mismatch");
      v.pop_back();Check(!resource_gpu::ParseDds(v,t,&d,&error),"truncated unused tail rejected");
    }
    std::vector<unsigned char>v=Make(0);Put(v,112,0x200);Check(!resource_gpu::ParseDds(v,0,&d,&error),"cubemap refused");Put(v,112,0);Put(v,28,5);Check(!resource_gpu::ParseDds(v,0,&d,&error),"underflow refused");
    v=Make(0);Put(v,16,0xffffffff);Check(!resource_gpu::ParseDds(v,0,&d,&error),"huge dimensions refused");
    v=Make(0);Put(v,84,0x30315844);Check(!resource_gpu::ParseDds(v,0,&d,&error),"DX10 refused");
    v=Make(0);v.push_back(0);Check(!resource_gpu::ParseDds(v,0,&d,&error),"extra bytes refused");
    Check(!resource_gpu::ParseDds(std::vector<unsigned char>(127),0,&d,&error),"short header refused");
    std::puts("DDS production parser PASS");return 0;
}
