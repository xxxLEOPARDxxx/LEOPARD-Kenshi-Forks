// Hardware skinning shader
#include "gbuffer.hlsl"
#include "wet.hlsl"

#ifdef RTW
#include "rtwshadows.hlsl"
#endif


#define WEIGHTS 3
#define BONES 60

#define DARKEN 0.8

void shadow_vs (    
	float4 position : POSITION,
	float2 texCoord : TEXCOORD0,

	uint4 blendIdx : BLENDINDICES,
	float4 blendWgt : BLENDWEIGHT,

	out float4 oPosition : POSITION,
	out float  oDepth    : TEXCOORD0,
	out float2 oTexCoord : TEXCOORD1,
	
	#ifdef RTW
	uniform sampler2D warpMap,
	#endif

	uniform float3x4  worldMatrix3x4Array[BONES],
	uniform float4x4  viewProjectionMatrix
) {
	// transform by indexed matrix
	float4 blendPos = float4(0,0,0,0);
	for (int i = 0; i < WEIGHTS; ++i) {
		blendPos += float4(mul(worldMatrix3x4Array[ blendIdx[i] ], position).xyz, 1.0) * blendWgt[i];
	}
	oPosition = mul(viewProjectionMatrix, blendPos);
	
	#ifdef RTW
	oPosition = GetDistortedPosition(warpMap, oPosition);
	#endif
	
	oPosition.z = max(oPosition.z, 0.0); // Avoid clipping
	oTexCoord = texCoord;
	oDepth = oPosition.z / oPosition.w;
}

// ------------------------------------------------------------------------------------------------------------------- //

void main_vs(    
		float4 position : POSITION,
		float2 texCoord : TEXCOORD0, 
		float3 normal   : NORMAL,
		float4 tangent  : TANGENT0,
		float3 binormal : BINORMAL0,
		
		uint4 blendIdx : BLENDINDICES,
		float4 blendWgt : BLENDWEIGHT,
		
		#if defined(PART_MASK) || defined(BLOOD)
		uint2 partData : TEXCOORD1,
		#endif
		#ifdef BLOOD
		uniform float4 bloodAmount[2],
		uniform float2 bloodScale,
		#endif
		#ifdef PART_MASK
		uniform int hiddenMask,
		#endif
		
		out	float4 oPosition : POSITION,
		out	float2 oTexCoord : TEXCOORD0,
		out	float3 oNormal   : TEXCOORD1,
		out	float3 oBinormal : TEXCOORD2,
		out	float3 oTangent  : TEXCOORD3,
		out	float3 oCamDir   : TEXCOORD4,
		out	float4 oWorldPos : TEXCOORD5,
		out float3 oWet      : TEXCOORD6,
		#ifdef BLOOD
		out float4 oBlood    : TEXCOORD7,
		#endif
		
		#ifdef COLLAPSE_SIDE
		uniform float2    hideSide,
		#endif

                uniform bool overrideDepth,
		
		uniform float3x4  worldMatrix3x4Array[BONES],
		uniform float4x4  viewProjectionMatrix,
		uniform float4x4  worldMatrix,
		uniform float3    waterLine
		
) {
	//binormal = cross(tangent.xyz, normal.xyz).xyz; // Not needed
	// transform by indexed matrix
	float4 blendPos      = float4(0,0,0,0);
	float3 blendNormal   = float3(0,0,0);
	float3 blendTangent  = float3(0,0,0);
	float3 blendBinormal = float3(0,0,0);
	for (int i = 0; i < WEIGHTS; ++i) {
		blendPos += float4(mul(worldMatrix3x4Array[ blendIdx[i] ], position).xyz, 1.0) * blendWgt[i];
		
		blendNormal   += mul((float3x3)worldMatrix3x4Array[blendIdx[i]], normal) * blendWgt[i];
		blendTangent  += mul((float3x3)worldMatrix3x4Array[blendIdx[i]], tangent.xyz) * blendWgt[i];
		blendBinormal += mul((float3x3)worldMatrix3x4Array[blendIdx[i]], binormal) * blendWgt[i];
	}
	
	oPosition = mul(viewProjectionMatrix, blendPos);
	oWorldPos = blendPos / blendPos.w;
	oCamDir   = float3(0,1,0); // HACK
	oTexCoord = texCoord;
	oNormal   = blendNormal;
	oTangent  = blendTangent;
	oBinormal = blendBinormal * tangent.w;
	
	oWet.x = max(waterLine.z, saturate(waterLine.x - position.y) * waterLine.y);
	oWet.yz = oCamDir.xx; // cus it is zero
	
	// Boot hiding
	#ifdef COLLAPSE_SIDE
	oPosition.xyz *= position.x<0? hideSide.x: hideSide.y;
	#endif
	
	#ifdef BLOOD	// Blood amount
	// Blood test - cylinder projection - used defines to select projection.
	oBlood.x = position.y * bloodScale.y; // 0.15;
	oBlood.y = atan2(normal.x, normal.z) / 3.14159265359 * 0.5;
	oBlood.z = oBlood.y>0? oBlood.y-0.5: oBlood.y+0.5;	// To fix wrapping
	oBlood.w = normal.z;
	oBlood.yz *= bloodScale.x;
	oWet.y = ((float[4])(bloodAmount[partData.x/4]))[partData.x%4];	// blood amount
	#endif
	#ifdef PART_MASK // Hidden parts
	oWet.z = (partData.y & hiddenMask) == 0? 0.0: 1.0;
	#endif
	if(overrideDepth)
		oPosition.z = 0.0;
	
}

// ------------------------------------------------------------------------------------------------------------------- //

void main_fs (
		float4 fragCoord: VPOS,
		float2 texCoord : TEXCOORD0,
		float3 normal   : TEXCOORD1,
		float3 binormal : TEXCOORD2,
		float3 tangent  : TEXCOORD3,
		float3 camDir   : TEXCOORD4,
		float4 worldPos : TEXCOORD5,
		float3 wet      : TEXCOORD6,
		#ifdef BLOOD
		float4 blood    : TEXCOORD7,
		#endif
		
		uniform sampler2D diffuseMap : register(s0),
		uniform sampler2D normalMap  : register(s1),
		uniform sampler2D colorMap   : register(s2),
		
		#ifdef BLOOD
		uniform sampler2D bloodMap   : register(s3),
		#endif
		
		uniform float farClip,
		uniform float3 cameraPos,
		uniform float glossMult,
		uniform float waterHeightRel,
		
		uniform float4 color1,
		uniform float4 color2,
		
		out GBuffer buffer
) {
	
	float4 diffuse    = tex2D(diffuseMap, texCoord);
	float4 normalTex  = tex2D(normalMap, texCoord);
	clip(normalTex.a - 0.6);
	
	// Colour map has colourisation data and metalness
	float3 cmap = tex2D(colorMap, texCoord).rgb;
	float metalness = cmap.b;
	
	// colourisation
	#ifdef COLOURING
	float intensity = (diffuse.r + diffuse.g + diffuse.b) / 3.0;
	float m1 = lerp(intensity, 1, color1.a);
	float m2 = lerp(intensity, 1, color2.a);
	diffuse.rgb = lerp(diffuse.rgb, color1.rgb * m1, cmap.r);
	diffuse.rgb = lerp(diffuse.rgb, color2.rgb * m2, cmap.g);
	#endif
	
	float3x3 tbn = float3x3(normalize(tangent), normalize(binormal), normalize(normal));
	normalTex.g = 1.0f - normalTex.g;
	float3 bump = normalTex.rgb * 2.0 - 1.0;
	float3 norm = mul(transpose(tbn), bump);
	
	// Blood
	#ifdef BLOOD
	float2 bloodCoord = blood.xy;
	if(blood.w < 0) bloodCoord.y = blood.z;
	float4 bloodCol = tex2D(bloodMap, bloodCoord);
	float bv = bloodCol.a - 1.0 + saturate(wet.y);
	diffuse.rgb = lerp(diffuse.rgb, bloodCol.rgb, saturate(bv));
	#endif
	
	// Wetness
	diffuse.a *= glossMult;
	float absorb = 1.0 - diffuse.a * 2.0;
	makeWet(diffuse, wet.x, absorb, waterHeightRel - worldPos.y, 0.5);

	// Write to GBuffer
	INITIALISE_OUTPUT( buffer );
	writeAlbedo   ( buffer, diffuse.rgb * DARKEN, fragCoord.xy );
	writeMetalness( buffer, metalness );
	writeGloss    ( buffer, diffuse.a );
	writeNormal   ( buffer, norm );
	writeDepth    ( buffer, length(worldPos - cameraPos) / farClip );
}
