#pragma once
namespace HoverOutlineShaders {
static const char vertex[] =
"struct Out {float4 p:SV_POSITION;float2 uv:TEXCOORD0;};\n"
"Out main_vs(float4 p:POSITION,float2 uv:TEXCOORD0){Out o;o.p=p;o.uv=uv;return o;}\n";
// Mask map: rgb = the character's relation colour, a = its radial distance
// normalised to 500 m (a per-binding constant; the native farClip parameter is
// an auto constant and cannot be trusted). Depth map: the main scene GBuffer
// buf2 (radial depth, R32F), read in metres via sceneFar. Inside the mask a
// soft tint is drawn only where the scene is not in front of the actor;
// outside it the outline fades outward over radius pixels and is never
// depth-tested, so it stays visible through walls. rectList clips the pass to
// the projected character boxes, so the search never runs screen-wide.
static const char fragment[] =
"Texture2D maskMap:register(t0);Texture2D depthMap:register(t1);float radius;float brightness;float sceneFar;float rectCount;float4 rectList[8];\n"
"float4 covered(int2 p){float4 m=maskMap.Load(int3(p,0));return m.a>0?m:float4(0,0,0,0);}\n"
"bool inRects(float2 p){for(int i=0;i<8;++i){if(float(i)>=rectCount)break;float4 r=rectList[i];if(p.x>=r.x && p.x<r.z && p.y>=r.y && p.y<r.w)return true;}return false;}\n"
"float4 main_fs(float4 pos:SV_POSITION,float2 uv:TEXCOORD0):SV_Target0{\n"
"if(!inRects(pos.xy))discard;\n"
"int2 p=int2(pos.xy);float4 m=covered(p);\n"
"if(m.a>0){\n"
"bool boundary=false;\n"
"for(int y=-1;y<=1 && !boundary;++y)for(int x=-1;x<=1 && !boundary;++x){if(x==0&&y==0)continue;float4 n=covered(p+int2(x,y));if(n.a<=0 || any(abs(n.rgb-m.rgb)>0.01))boundary=true;}\n"
"if(boundary)return float4(m.rgb,saturate(0.6*brightness));\n"
"float scene=depthMap.Load(int3(p,0)).r*sceneFar;if(scene>0 && m.a*500.0>scene+2.0)discard;return float4(m.rgb,0.2*brightness);}\n"
"int r=int(ceil(radius));int dg=int(radius*0.7071+0.5);\n"
"if(covered(p+int2(r,0)).a<=0 && covered(p+int2(-r,0)).a<=0 && covered(p+int2(0,r)).a<=0 && covered(p+int2(0,-r)).a<=0 &&\n"
"   covered(p+int2(dg,dg)).a<=0 && covered(p+int2(-dg,dg)).a<=0 && covered(p+int2(dg,-dg)).a<=0 && covered(p+int2(-dg,-dg)).a<=0)discard;\n"
"float best=1e9;float4 edge=float4(0,0,0,0);\n"
"for(int y=-r;y<=r;++y)for(int x=-r;x<=r;++x){float4 n=covered(p+int2(x,y));if(n.a>0){float d=sqrt(float(x*x+y*y));if(d<best){best=d;edge=n;}}}\n"
"if(best>radius+0.5)discard;\n"
"float a=saturate(0.6*brightness)*saturate(1.0-(best-1.0)/(radius+0.5));\n"
"return float4(edge.rgb,a);}\n";
// Minimal fragment for materials without alpha-discard: constant colour and
// the binding's depth, no texture reads. The native vertex program is kept
// verbatim, so skinning and part hiding are preserved. Derived per-material
// fragments remain only for shaders that alpha-clip (hair and feather cards).
static const char maskTrivial[] =
"uniform float4 leopardMaskColour;uniform float leopardMaskDepth;\n"
"float4 main_fs():SV_Target0{return float4(leopardMaskColour.rgb,leopardMaskDepth);}\n";
}
