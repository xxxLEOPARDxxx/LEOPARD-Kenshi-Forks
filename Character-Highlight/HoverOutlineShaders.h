#pragma once
namespace HoverOutlineShaders {
static const char vertex[] =
"struct Out {float4 p:SV_POSITION;float2 uv:TEXCOORD0;};\n"
"Out main_vs(float4 p:POSITION,float2 uv:TEXCOORD0){Out o;o.p=p;o.uv=uv;return o;}\n";
static const char fragment[] =
"Texture2D maskMap:register(t0);Texture2D depthMap:register(t1);SamplerState maskSampler:register(s0);SamplerState depthSampler:register(s1);float2 pixel;float radius;float brightness;\n"
"float4 visibleMask(float2 uv){float4 m=maskMap.SampleLevel(maskSampler,uv,0);float d=depthMap.SampleLevel(depthSampler,uv,0).r;return m.a>0 && m.a<=d+0.0003?m:0;}\n"
"float4 main_fs(float2 uv:TEXCOORD0):SV_Target0{float4 c=visibleMask(uv);clip(0.00001-c.a);float4 edge=0;\n"
"for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){float4 n=visibleMask(uv+float2(x,y)*pixel*radius);if(n.a>edge.a)edge=n;}\n"
"clip(edge.a-0.00001);return float4(edge.rgb*brightness,0.8);}\n";
}
