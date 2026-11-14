#include "resources/resource_dds.h"
#include <cstring>
namespace resource_gpu {
namespace {
unsigned U32(const std::vector<unsigned char>&b,size_t o){unsigned n;std::memcpy(&n,&b[o],4);return n;}
bool Fail(std::string*e,const char*s){if(e)*e=s;return false;}
}
bool ParseDds(const std::vector<unsigned char>&b,unsigned type,Dds*out,std::string*error){
    if(!out)return Fail(error,"null output");*out=Dds();
    if(type>1||b.size()<128||b.size()>256ULL*1024*1024)return Fail(error,"unsupported DDS size/type");
    if(U32(b,0)!=0x20534444||U32(b,4)!=124||U32(b,76)!=32)return Fail(error,"invalid DDS header");
    if((U32(b,8)&0x21007)!=0x21007||U32(b,80)!=4||U32(b,84)!=(type?0x31545844u:0x35545844u))return Fail(error,"legacy BC1/BC3 required");
    if((U32(b,108)&0x401008)!=0x401008||U32(b,112)||U32(b,116)||U32(b,120)||U32(b,124)||U32(b,24)>1)return Fail(error,"non-flat DDS");
    unsigned w=U32(b,16),h=U32(b,12),count=U32(b,28);
    if(w!=h||w<256||w>8192||(w&(w-1)))return Fail(error,"unsupported dimensions");
    unsigned full=1;for(unsigned n=w;n>1;n>>=1)++full;
    if(count!=full||count<6)return Fail(error,"full mip chain required");
    Dds result;result.width=w;result.height=h;result.levels=count-5;result.format=type?71:77;
    size_t offset=128;
    for(unsigned m=0;m<count;++m){
        unsigned blocks=(w+3)/4,pitch=blocks*(type?8:16);size_t bytes=size_t(pitch)*((h+3)/4);
        if(offset>b.size()||bytes>b.size()-offset)return Fail(error,"truncated mip payload");
        if(m<result.levels){DdsMip span={offset,bytes,w,h,pitch};result.mips.push_back(span);}
        offset+=bytes;w=w>1?w/2:1;h=h>1?h/2:1;
    }
    if(offset!=b.size())return Fail(error,"unexpected trailing bytes");
    *out=result;if(error)error->clear();return true;
}
}
