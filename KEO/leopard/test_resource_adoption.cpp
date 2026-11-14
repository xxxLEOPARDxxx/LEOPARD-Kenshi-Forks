#include "resources/resource_adoption_policy.h"
#include <cassert>
#include <iostream>
int main(){
    D3D11_TEXTURE2D_DESC a={};a.Width=a.Height=2048;a.ArraySize=6;a.MipLevels=7;a.Format=DXGI_FORMAT_BC3_UNORM;a.SampleDesc.Count=1;a.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_TEXTURE2D_DESC b=a;assert(resource_adoption::SameDesc(a,b));b.MipLevels=8;assert(!resource_adoption::SameDesc(a,b));b=a;b.Format=DXGI_FORMAT_BC3_UNORM_SRGB;assert(!resource_adoption::SameDesc(a,b));
    unsigned char original[32]={0},native[32]={0};resource_gpu::Span span={original,32,16,8,4};
    assert(resource_adoption::Matches(span,NULL,native,16,0));assert(resource_adoption::Matches(span,NULL,native,16,32));
    native[31]=1;assert(!resource_adoption::Matches(span,NULL,native,16,32));native[31]=0;
    D3D11_BOX box={};assert(!resource_adoption::Matches(span,&box,native,16,32));
    box.right=8;box.bottom=4;box.back=1;
    assert(resource_adoption::Matches(span,&box,native,16,0));
    box.left=1;assert(!resource_adoption::Matches(span,&box,native,16,0));box.left=0;
    box.right=4;assert(!resource_adoption::Matches(span,&box,native,16,0));box.right=8;
    box.back=2;assert(!resource_adoption::Matches(span,&box,native,16,0));box.back=1;
    box.top=1;assert(!resource_adoption::Matches(span,&box,native,16,0));box.top=0;
    box.front=1;assert(!resource_adoption::Matches(span,&box,native,16,0));
    assert(!resource_adoption::Matches(span,NULL,native,32,32));assert(!resource_adoption::Matches(span,NULL,NULL,16,32));
    std::cout<<"native GPU adoption policy passed\n";
}
