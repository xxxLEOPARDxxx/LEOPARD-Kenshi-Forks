#include "gbuffer.hlsl"
#include "wet.hlsl"

#define DARKEN 0.7

// Umm, Ill just leave this as it was...
float3 colorise( float3 base, float3 coloring, float3 tone, float amount )
{
     //colorise
     float3 c = coloring * amount;
          c += ( base.r + base.g + base.b - c.r - c.g - c.b ) / 3.0;

     // Saturation
     float bright = ( c.r + c.g + c.b ) / 3.0;
     c = ( c - bright ) * tone.x + bright;

     //contrast
     //c = lerp(c, c*bright*2.0, tone.z);

     //brightness
     c += tone.y;

     return c;
}

void main_fs(
     float4 fragCoord: VPOS,
     float2 texCoord : TEXCOORD0,
     float3 normal   : TEXCOORD1,
     float3 binormal : TEXCOORD2,
     float3 tangent  : TEXCOORD3,
     float3 camDir   : TEXCOORD4,
     float4 worldPos : TEXCOORD5,
     float3 wet      : TEXCOORD6,
     float4 blood    : TEXCOORD7,

     uniform sampler2D diffuseMap       : register( s0 ),
     uniform sampler2D normalMap        : register( s1 ),
     uniform sampler2D colorMap         : register( s2 ),
     
     uniform sampler2D maskMap          : register( s3 ),
     uniform sampler2D blendNormalMap   : register( s4 ),
     
     uniform sampler2D headDiffuseMap   : register( s5 ),
     uniform sampler2D headNormalMap    : register( s6 ),
     uniform sampler2D headMaskMap      : register( s7 ),
     
     uniform sampler2D hairMap          : register( s8 ),
     uniform sampler2D beardMap         : register( s9 ),

     uniform sampler2D vestDiffuseMap   : register( s10 ),
     uniform sampler2D vestNormalMap    : register( s11 ),
     
     uniform sampler2D bloodMap         : register( s12 ),

     uniform float3 skintone,
     uniform float4 color,				// shirt colour
     uniform float4 coloroverride,
     uniform float  muscleBlend,
     uniform float3 cameraPos,
     uniform float  farClip,
     uniform float  waterHeightRel,
     uniform float4 bloodColour,

     uniform float3 hairColor,
     uniform float4 hairMult,
     uniform float4 hairAlpha,
     uniform float4 beardAlpha,

     out GBuffer buffer
) {
     // Body texture
     float4 body = tex2D( diffuseMap, texCoord );
     float4 bodyN = tex2D( normalMap, texCoord );
     float4 vestC = tex2D( colorMap, texCoord );		// This needs to be up here SM4 compatability
     
     // Part map
     clip( -wet.z );
     
     float skinToneMask = tex2D( maskMap, texCoord ).r;
     bodyN = lerp( bodyN, tex2D( blendNormalMap, texCoord ), muscleBlend );
     
     // Head
     texCoord.y += 1.0;
     body += tex2D( headDiffuseMap, texCoord );
     bodyN += tex2D( headNormalMap, texCoord );

     // Skin Tone
     float2 headMask = tex2D( headMaskMap, texCoord ).rg;
     skinToneMask += headMask.r;
     body.rgb *= 1.0f - ( skintone * skinToneMask );
     

     // Hair
     float4 hair = tex2D( hairMap, texCoord );
     float4 beard = tex2D( beardMap, texCoord );
     
     float hairValue  = saturate( dot( hair, hairAlpha ) );
     float beardValue = saturate( dot( beard, beardAlpha ) );

     float3 hairCol = hairColor * saturate( dot( hair, hairMult ) );
     float3 beardCol = hairColor * beardValue;

     body.rgb = lerp( body.rgb, hairCol, hairValue );
     body.rgb = lerp( body.rgb, beardCol, beardValue );

     const float sweat = 1.0;
     float gloss = body.a * sweat;

     // Add Clothing
     texCoord.y -= 1.0;
     float4 vest = tex2D( vestDiffuseMap, texCoord );
     float4 vestN = tex2D( vestNormalMap, texCoord );
     float intensity = dot(vest.rgb, 1) / 3.0;
     vest.rgb = lerp( vest.rgb, color.rgb * intensity, vestC.r );
     body.rgb = lerp( body.rgb, vest.rgb, vestN.a );
     bodyN.wy = lerp( bodyN.wy, vestN.xy, vestN.a );
     gloss = lerp( gloss, vest.a, vestN.a );
     
     
     // Blood
     //#ifdef BLOOD
     float2 bloodCoord = blood.xy;
     if(blood.w < 0) bloodCoord.y = blood.z;
     float4 bloodCol = tex2D(bloodMap, bloodCoord) * bloodColour;
     //float bv = blood.w * bloodCol.a;
     float bv = bloodCol.a - 1.0 + saturate(wet.y);
     body.rgb = lerp(body.rgb, bloodCol.rgb, saturate(bv));
     
     
     //#endif

     // Expand DXT5 Normal format
     bodyN.x = bodyN.w;
     //bodyN.y = 1.0f - bodyN.y;
     bodyN.xy = ( bodyN.xy * 2.0f ) - 1.0f;
     bodyN.z = sqrt( 1.0f - dot( bodyN.xy, bodyN.xy ) );
     
     body.a = gloss;
     float absorb = 1.0 - gloss * 2.0;
     float clothing = saturate(vestN.a + 1-skinToneMask);
     //absorb = lerp(0.2, absorb, clothing);	// lower skin absorbance
     absorb *= clothing * 0.8 + 0.2;	// skin absorbance lower than clothing
     makeWet(body, wet.x, absorb, waterHeightRel - worldPos.y, 0.5);
     gloss = body.a;
     
     // Stop texture hair being super shiny
     gloss *= lerp(1.0, 0.4, max(hairValue, beardValue));

     float3x3 tbn = float3x3( normalize( tangent ), normalize( binormal ), normalize( normal ) );
     float3 worldNormal = mul( transpose( tbn ), bodyN.xyz );

     body.rgb = lerp(body.rgb, coloroverride.rgb, coloroverride.a);

     // Write to GBuffer
     INITIALISE_OUTPUT( buffer );
     writeAlbedo   ( buffer, body.rgb * DARKEN, fragCoord.xy );
     writeMetalness( buffer, 0.0f );
     writeGloss    ( buffer, gloss );
     writeNormal   ( buffer, worldNormal );
     writeDepth    ( buffer, length( worldPos.xyz - cameraPos ) / farClip );
          
     // Glowy eyes
     buffer.buf1.a = headMask.g * 0.5 + 0.5;
     buffer.buf1.a *= saturate(headMask.g * 255); // mul_sat
}

void distant_fs(
     float4 fragCoord: VPOS,
     float2 texCoord : TEXCOORD0,
     float3 normal   : TEXCOORD1,
     float3 meh1     : TEXCOORD2,
     float3 meh2     : TEXCOORD3,
     float3 meh3     : TEXCOORD4,
     float4 worldPos : TEXCOORD5,

     uniform sampler2D diffuseMap       : register( s0 ),
     uniform sampler2D headDiffuseMap   : register( s1 ),
     uniform sampler2D vestDiffuseMap   : register( s2 ),
     uniform sampler2D vestNormalMap    : register( s3 ),
     uniform sampler2D vestColorMap     : register( s4 ),

     uniform float3 skintone,
     uniform float3 color,
     uniform float4 coloroverride,
     uniform float3 tone,
     uniform float3 cameraPos,
     uniform float farClip,

     out GBuffer buffer
) {
     // Body texture
     float4 body = tex2D( diffuseMap, texCoord );

     // Vest
     float4 vest = tex2D( vestDiffuseMap, texCoord );
     float4 vestC = tex2D( vestColorMap, texCoord );
     float  vestAlpha = tex2D( vestNormalMap, texCoord ).a;

     // Head
     texCoord.y += 1.0;
     body += tex2D( headDiffuseMap, texCoord );

     // Skin Tone
     body.rgb *= 1.0f - ( ( 1.0f - skintone.rgb ) );

     const float sweat = 0.0;
     float specular = body.a * sweat;

     // Add Clothing
     float3 vestColor = colorise( vest.rgb, color, tone, vestC.r );
          vest.rgb = lerp( vest.rgb, vestColor, vestC.r );
     body.rgb = lerp( body.rgb, vest.rgb, vestAlpha );
     specular = lerp( specular, vest.a, vestAlpha );

     body.rgb = lerp(body.rgb, coloroverride.rgb, coloroverride.a);

     // Write to GBuffer
     INITIALISE_OUTPUT( buffer );
     writeAlbedo   ( buffer, body.rgb * DARKEN, fragCoord.xy );
     writeMetalness( buffer, 0.0f );
     writeGloss    ( buffer, specular );
     writeNormal   ( buffer, normal );
     writeDepth    ( buffer, length( worldPos.xyz - cameraPos ) / farClip );
}

void zero_fs(
     float4 fragCoord: VPOS,
     float3 normal : TEXCOORD1,
     float4 worldPos : TEXCOORD5,
     uniform float3 cameraPos,
     uniform float farClip,
     out GBuffer buffer
) {
     // Write to GBuffer
     INITIALISE_OUTPUT( buffer );
     writeAlbedo   ( buffer, float3( 0.3f, 0.25f, 0.1f ), fragCoord.xy );
     writeMetalness( buffer, 0.0f );
     writeGloss    ( buffer, 0.0f );
     writeNormal   ( buffer, normal );
     writeDepth    ( buffer, length( worldPos.xyz - cameraPos ) / farClip );
}

void hair_fs(
     float4 fragCoord: VPOS,
     float2 texCoord : TEXCOORD0,
     float3 normal   : TEXCOORD1,
     float3 binormal : TEXCOORD2,
     float3 tangent  : TEXCOORD3,
     float3 camDir   : TEXCOORD4,
     float4 worldPos : TEXCOORD5,

     uniform sampler2D diffuseMap  : register( s0 ),
     uniform sampler2D normalMap   : register( s1 ),
     uniform sampler2D colorMap    : register( s2 ),

     uniform float3 color,
     uniform float4 diffuseChannel,
     uniform float4 alphaChannel,
     uniform float3 threshold,
     uniform float3 cameraPos,
     uniform float  farClip,
     uniform float  specularMult,

     out GBuffer buffer
) {
     // Colouring
     float4 hair = tex2D( diffuseMap, texCoord );
     float4 d = hair * diffuseChannel;
     float4 a = hair * alphaChannel;
     float brightness = d.r + d.g + d.b + d.a;
     float alpha = a.r + a.g + a.b + a.a;
     clip( alpha - threshold );

     // Write to GBuffer
     INITIALISE_OUTPUT( buffer );
     writeAlbedo   ( buffer, brightness * color * DARKEN, fragCoord.xy );
     writeMetalness( buffer, 0.0f );
     writeGloss    ( buffer, brightness * specularMult );
     writeNormal   ( buffer, normalize( normal ) );
     writeDepth    ( buffer, length( worldPos.xyz - cameraPos ) / farClip );
}

// -------------------------------------------------------------------------------------------------------- //

void severed_limb_vs (
     float4 position : POSITION,
     float4 normal   : NORMAL,
     float2 uv       : TEXCOORD0,
     float4 tangent  : TANGENT,

     out float4 oPosition : SV_Position,
     out float2 oTexCoord : TEXCOORD0,
     out float3 oNormal   : TEXCOORD1,
     out float3 oBinormal : TEXCOORD2,
     out float3 oTangent  : TEXCOORD3,
     out float3 oCameraDir: TEXCOORD4,
     out float3 oWorldPos : TEXCOORD5,
     out float4 oBlood    : TEXCOORD6,

     uniform float3 cameraDir,
     uniform float4x4 worldViewProjMatrix,
     uniform float4x4 worldMatrix
){
     float3 binormal = cross(tangent.xyz, normal.xyz); // Generate binormal
     binormal *= tangent.w; // Fix mirrored faces
     oWorldPos = mul(worldMatrix, position).xyz;
     oPosition = mul(worldViewProjMatrix, position);
     oTexCoord = uv;
     oNormal   = mul((float3x3)worldMatrix, normal.xyz);
     oBinormal = mul((float3x3)worldMatrix, binormal.xyz);
     oTangent  = mul((float3x3)worldMatrix, tangent.xyz);
     oCameraDir= mul((float3x3)worldMatrix, cameraDir.xyz);
     // Blood mapping
     oBlood.x = position.x * 0.3;
     oBlood.y = atan2(normal.y, normal.z) / 3.14159265359 * 0.5;
     oBlood.z = oBlood.y > 0? oBlood.y-0.5: oBlood.y+0.5;
     oBlood.w = normal.z;
}


void severed_limb_fs (
     float4 fragCoord: VPOS,
     float2 texCoord : TEXCOORD0,
     float3 normal   : TEXCOORD1,
     float3 binormal : TEXCOORD2,
     float3 tangent  : TEXCOORD3,
     float3 camDir   : TEXCOORD4,
     float4 worldPos : TEXCOORD5,
     float4 bloodUV  : TEXCOORD6,
     
     uniform sampler2D diffuseMap  : register( s0 ),
     uniform sampler2D normalMap   : register( s1 ),
     uniform sampler2D maskMap     : register( s3 ),
     uniform sampler2D bloodMap    : register( s4 ),
     
     uniform float3 cameraPos,
     uniform float  farClip,
     uniform float  waterHeightRel,
     uniform float4 skinTone,
     uniform float4 bloodColour,
     
     out GBuffer buffer
) {
     float4 body = tex2D( diffuseMap, texCoord );
     float4 bodyN = tex2D( normalMap, texCoord );
     float skinToneMask = tex2D( maskMap, texCoord ).r;
     body.rgb *= 1.0f - skinTone.rgb * skinToneMask;
     
     float gloss = body.a;
     
     // Bloodyness
     float2 bloodCoord = bloodUV.w < 0 ? bloodUV.xz: bloodUV.xy;
     float4 blood = tex2D(bloodMap, bloodCoord) * bloodColour;
     blood.a += max(-bloodUV.x*2, 0);
     body.rgb = lerp(body.rgb, blood.rgb, min(blood.a,1));
     
     // expand dxt5 normal map
     bodyN.x = bodyN.w;
     bodyN.xy = ( bodyN.xy * 2.0f ) - 1.0f;
     bodyN.z = sqrt( 1.0f - dot( bodyN.xy, bodyN.xy ) );
     
     // Map normals
     float3x3 tbn = float3x3( normalize( tangent ), normalize( binormal ), normalize( normal ) );
     float3 worldNormal = mul( transpose( tbn ), bodyN.xyz );
     
     // Write to GBuffer
     INITIALISE_OUTPUT( buffer );
     writeAlbedo   ( buffer, body.rgb * DARKEN, fragCoord.xy );
     writeMetalness( buffer, 0.0f );
     writeGloss    ( buffer, gloss );
     writeNormal   ( buffer, worldNormal );
     writeDepth    ( buffer, length( worldPos.xyz - cameraPos ) / farClip );
}
