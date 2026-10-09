#include "gbuffer.hlsl"
#include "wet.hlsl"

void main_fs (
		float4 fragCoord: VPOS,
		float2 texCoord : TEXCOORD0,
		float3 normal   : TEXCOORD1,
		float3 binormal : TEXCOORD2,
		float3 tangent  : TEXCOORD3,
		float3 camDir   : TEXCOORD4,
		float4 worldPos : TEXCOORD5,
		float3 wet      : TEXCOORD6,
		float4 blood    : TEXCOORD7,
		
		uniform sampler2D diffuseMap : register(s0),
		uniform sampler2D normalMap  : register(s1),
		uniform sampler2D metalMap   : register(s2),
		
		#ifdef BLOOD
		uniform sampler2D bloodMap   : register(s3),
		uniform float4 bloodColour,
		#endif
		
		uniform float farClip,
		uniform float3 cameraPos,
		uniform float glossMult,
                uniform float4 coloroverride,
		uniform float waterHeightRel,
		
		out GBuffer buffer
) {
	
	float4 diffuse    = tex2D(diffuseMap, texCoord);
	float4 normalTex  = tex2D(normalMap, texCoord);
	float3 metalness = tex2D(metalMap, texCoord).r;
	
	#ifdef DXT5NORMAL
	normalTex.x  = normalTex.w;
	normalTex.y  = normalTex.y;
	normalTex.xy = normalTex.xy * 2.0 - 1.0;
	normalTex.z = sqrt( 1.0f - dot( normalTex.xy, normalTex.xy ) );
	#else
	normalTex.xyz = normalTex.xyz * 2.0 - 1.0;
	#endif
	
	normalTex.g = - normalTex.g;	// Flip green ?
	float3x3 tbn = float3x3(normalize(tangent), normalize(binormal), normalize(normal));
	float3 norm = mul(transpose(tbn), normalTex.xyz);
	
	// Blood
	#ifdef BLOOD
	float2 bloodCoord = blood.xy;
	if(blood.w < 0) bloodCoord.y = blood.z;
	float4 bloodCol = tex2D(bloodMap, bloodCoord) * bloodColour;
	float bv = bloodCol.a - 1.0 + saturate(wet.y);
	diffuse.rgb = lerp(diffuse.rgb, bloodCol.rgb, saturate(bv));
	#endif
	
	
	// Wetness
	diffuse.a *= glossMult;
	float absorb = 1.0 - diffuse.a * 2.0;
	makeWet(diffuse, wet.x, absorb, waterHeightRel - worldPos.y, 0.5);

        diffuse.rgb = lerp(diffuse.rgb, coloroverride.rgb, coloroverride.a);

	// Write to GBuffer
	INITIALISE_OUTPUT( buffer );
	writeAlbedo   ( buffer, diffuse.rgb, fragCoord.xy );
	writeMetalness( buffer, metalness );
	writeGloss    ( buffer, diffuse.a );
	writeNormal   ( buffer, norm );
	writeDepth    ( buffer, length(worldPos - cameraPos) / farClip );
}