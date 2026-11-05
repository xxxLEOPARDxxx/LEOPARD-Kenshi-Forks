#pragma once
namespace HoverOutlineShaders {
// Independent shaders: do not replace the game's files or other mods' programs.
static const char vertex[] =
"float3x4 bones[60]; float4x4 worldMatrix; float4x4 viewProjection; float4 viewport;\n"
"float outlinePixels; int hiddenMask; float2 hideSide;\n"
"struct In { float4 p:POSITION; float3 n:NORMAL;\n#ifdef SKINNED\n uint4 ids:BLENDINDICES; float4 w:BLENDWEIGHT;\n#endif\n"
"#ifdef MASKED\n uint2 part:TEXCOORD1;\n#endif\n };\n"
"struct Out { float4 p:SV_POSITION; float3 world:TEXCOORD0; float3 normal:TEXCOORD1; float visible:TEXCOORD2; };\n"
"Out main_vs(In v) { Out o; float3 p=0,n=0;\n"
"#ifdef SKINNED\n for(int i=0;i<3;i++) {p+=mul(bones[v.ids[i]],v.p)*v.w[i]; n+=mul((float3x3)bones[v.ids[i]],v.n)*v.w[i];}\n"
"#else\n p=mul(worldMatrix,v.p).xyz; n=mul((float3x3)worldMatrix,v.n);\n#endif\n"
"o.world=p; o.normal=normalize(n); o.p=mul(viewProjection,float4(p,1));\n"
"float4 projected=mul(viewProjection,float4(n,0)); float2 dir=projected.xy*o.p.w-o.p.xy*projected.w;\n"
"dir*=rsqrt(max(dot(dir,dir),0.000001)); o.p.xy+=dir*viewport.zw*outlinePixels*2*o.p.w;\n"
"o.visible=1;\n#ifdef MASKED\n o.visible=(v.part.y & hiddenMask)==0?1:0;\n#endif\n"
"#ifdef COLLAPSE_SIDE\n o.visible*=v.p.x<0?hideSide.x:hideSide.y;\n#endif\n return o; }\n";
static const char fragment[] =
"float3 cameraPos; float farClip; float4 outlineColour; float outlineEnabled;\n"
"struct Out {float4 a:SV_Target0; float4 n:SV_Target1; float4 d:SV_Target2;};\n"
"Out main_fs(float4 pixel:SV_POSITION,float3 world:TEXCOORD0,float3 normal:TEXCOORD1,float visible:TEXCOORD2) {\n"
"clip(min(visible,outlineEnabled)-0.5); Out o; float3 c=outlineColour.rgb;\n"
"float y=dot(c,float3(0.25,0.5,0.25)); float co=0.5*c.r-0.5*c.b+0.5; float cg=-0.25*c.r+0.5*c.g-0.25*c.b+0.5;\n"
"o.a=float4(y,fmod(floor(pixel.x),2)==fmod(floor(pixel.y),2)?cg:co,0,0);\n"
"o.n=float4(normalize(normal)*0.5+0.5,0); o.d=(length(world-cameraPos)/farClip).xxxx; return o;}\n";
}
