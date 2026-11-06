#pragma once
namespace HoverOutlineShaders {
static const char vertex[] =
"struct Out {float4 p:SV_POSITION;float2 uv:TEXCOORD0;};\n"
"Out main_vs(float4 p:POSITION,float2 uv:TEXCOORD0){Out o;o.p=p;o.uv=uv;return o;}\n";
// Mask map: rgb = the character's relation colour, a = its radial depth
// (native writeDepth: length(worldPos-cameraPos)/farClip; 0 = nothing drawn).
// Depth map: the main scene GBuffer buf2 (same radial encoding, R32F).
// Inside the mask a soft tint is drawn only where the character is in front of
// the scene depth; outside it the outline fades outward over radius pixels and
// is never depth-tested, so it stays visible through walls. rectList clips the
// pass to the projected character boxes, so the search never runs screen-wide.
static const char fragment[] =
"Texture2D maskMap:register(t0);Texture2D depthMap:register(t1);float radius;float brightness;float invFarClip;float rectCount;float4 rectList[8];\n"
"float4 covered(int2 p){float4 m=maskMap.Load(int3(p,0));return m.a>0?m:float4(0,0,0,0);}\n"
"bool inRects(float2 p){for(int i=0;i<8;++i){if(float(i)>=rectCount)break;float4 r=rectList[i];if(p.x>=r.x && p.x<r.z && p.y>=r.y && p.y<r.w)return true;}return false;}\n"
"float4 main_fs(float4 pos:SV_POSITION,float2 uv:TEXCOORD0):SV_Target0{\n"
"if(!inRects(pos.xy))discard;\n"
"int2 p=int2(pos.xy);float4 m=covered(p);\n"
"if(m.a>0){float scene=depthMap.Load(int3(p,0)).r;if(scene>0 && m.a>scene*1.02+2*invFarClip)discard;return float4(m.rgb*brightness,0.2);}\n"
"int r=int(ceil(radius));float best=1e9;float4 edge=float4(0,0,0,0);\n"
"for(int y=-r;y<=r;++y)for(int x=-r;x<=r;++x){float4 n=covered(p+int2(x,y));if(n.a>0){float d=sqrt(float(x*x+y*y));if(d<best){best=d;edge=n;}}}\n"
"if(best>radius+0.5)discard;\n"
"float a=0.6*saturate(1.0-(best-1.0)/(radius+0.5));\n"
"return float4(edge.rgb*brightness,a);}\n";
}
