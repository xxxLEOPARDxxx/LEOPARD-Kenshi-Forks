#pragma once
#include "resources/resource_gpu_store.h"
#include <cstring>
namespace resource_adoption {
inline bool SameDesc(const D3D11_TEXTURE2D_DESC&a,const D3D11_TEXTURE2D_DESC&b){
    return a.Width==b.Width&&a.Height==b.Height&&a.MipLevels==b.MipLevels&&a.ArraySize==b.ArraySize
        &&a.Format==b.Format&&a.SampleDesc.Count==b.SampleDesc.Count&&a.SampleDesc.Quality==b.SampleDesc.Quality
        &&a.Usage==b.Usage&&a.BindFlags==b.BindFlags&&a.CPUAccessFlags==b.CPUAccessFlags&&a.MiscFlags==b.MiscFlags;
}
inline bool Matches(const resource_gpu::Span&s,const D3D11_BOX*box,const void*data,UINT row,UINT depth){
    bool full=!box||(box->left==0&&box->top==0&&box->front==0&&box->right==s.width&&box->bottom==s.height&&box->back==1);
    return full&&data&&s.data&&s.bytes&&row==s.rowPitch&&(depth==0||depth==s.bytes)
        &&std::memcmp(data,s.data,s.bytes)==0;
}
}
