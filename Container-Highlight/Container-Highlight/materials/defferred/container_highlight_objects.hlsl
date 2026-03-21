// Deferred object highlight shader for container tinting.
#include "gbuffer.hlsl"
#include "wet.hlsl"

void main_vs(
    float4 position : POSITION,
    float4 normal   : NORMAL,
    float2 uv       : TEXCOORD0,
    float4 tangent  : TANGENT,

    #ifdef INSTANCED
    float4 instanceMatrix0 : TEXCOORD1,
    float4 instanceMatrix1 : TEXCOORD2,
    float4 instanceMatrix2 : TEXCOORD3,
    #endif

    out float4 oPosition : SV_Position,
    out float2 oTexCoord : TEXCOORD0,
    out float3 oNormal   : TEXCOORD1,
    out float3 oBinormal : TEXCOORD2,
    out float3 oTangent  : TEXCOORD3,
    out float3 oCameraDir: TEXCOORD4,
    out float3 oWorldPos : TEXCOORD5,

    #ifdef CONSTRUCTION
    out float2 oConstruction : TEXCOORD6,
    uniform float2 upperPos,
    #endif

    #if defined(COLOURING) || defined(DUAL_TEXTURE)
    float4 colour : COLOR0,
    out float4 oColour : COLOR0,
    #endif

    uniform float2 tiling,
    uniform float3 cameraDir,
    uniform float4x4 worldViewProjMatrix,
    uniform float4x4 worldMatrix,
    uniform bool overrideDepth
) {
    float3 binormal = cross(tangent.xyz, normal.xyz);
    binormal *= tangent.w;

    #ifdef INSTANCED
    worldMatrix = float4x4(instanceMatrix0, instanceMatrix1, instanceMatrix2, float4(0,0,0,1));
    position = mul(worldMatrix, position);
    oWorldPos = position.xyz;
    #else
    oWorldPos = mul(worldMatrix, position).xyz;
    #endif

    oPosition = mul(worldViewProjMatrix, position);
    oTexCoord = uv * tiling;
    oNormal = mul((float3x3)worldMatrix, normal.xyz);
    oBinormal = mul((float3x3)worldMatrix, binormal);
    oTangent = mul((float3x3)worldMatrix, tangent.xyz);
    oCameraDir = mul((float3x3)worldMatrix, cameraDir.xyz);

    #if CONSTRUCTION
    oConstruction.x = (position.y - upperPos.y) / (upperPos.x - upperPos.y);
    oConstruction.y = position.y / upperPos.x;
    #endif

    #ifdef COLOURING
    oColour = colour;
    #endif

    if (overrideDepth)
    {
        oPosition.z = 0.0;
    }
}

SamplerState Nearest {
    Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    AddressU = Clamp;
    AddressV = Clamp;
};

void main_ps(
    float4 fragCoord: SV_Position,
    float2 texCoord : TEXCOORD0,
    float3 normal   : TEXCOORD1,
    float3 binormal : TEXCOORD2,
    float3 tangent  : TEXCOORD3,
    float3 cameraDir: TEXCOORD4,
    float3 worldPos : TEXCOORD5,

    #ifdef CONSTRUCTION
    float2 construction : TEXCOORD6,
    #endif

    #if defined(COLOURING) || defined(DUAL_TEXTURE)
    float4 colour : COLOR,
    #endif

    #if defined(DOUBLESIDED) || defined(DOUBLESIDED2)
    bool frontFace : SV_IsFrontFace,
    #endif

    SamplerState sampleState,
    Texture2D base_map    : register(s0),
    Texture2D base_normal : register(s1),
    Texture2D metal_map   : register(s2),

    #ifdef CONSTRUCTION
    Texture2D grid_map    : register(s3),
    #endif

    #ifdef DUAL_TEXTURE
    #ifdef CONSTRUCTION
    Texture2D diffuseMap2 : register(s4),
    Texture2D normalMap2  : register(s5),
    Texture2D metalMap2   : register(s6),
    #else
    Texture2D diffuseMap2 : register(s3),
    Texture2D normalMap2  : register(s4),
    Texture2D metalMap2   : register(s5),
    #endif
    #endif

    #ifdef CLIP_INTERIOR
    Texture2D interiorMask : register(s7),
    uniform float4 viewport,
    #endif

    #ifdef DUST
    Texture2D dustNoise : register(s8),
    uniform float4 dustColour,
    uniform float3 dustAmount,
    #endif

    #ifdef CONSTRUCTION
    uniform float4 constructionState,
    uniform float scaffoldTiling,
    #endif
    #ifdef TRANSPARENCY
    uniform float threshold,
    #endif
    #ifdef EMISSIVE
    uniform float brightness,
    #endif

    uniform float3 cameraPos,
    uniform float farClip,
    uniform float waterHeightRel,
    uniform float wetness,
    uniform float glossMult,
    uniform float4 coloroverride,

    out GBuffer buffer
) {
    #ifdef CLIP_INTERIOR
    float2 interior = interiorMask.Sample(Nearest, fragCoord.xy * viewport.zw);
    if (fragCoord.w < interior.y) clip(interior.x - fragCoord.w);
    #endif

    float4 diffuse = base_map.Sample(sampleState, texCoord);
    float4 normalTex = base_normal.Sample(sampleState, texCoord);
    float metalness = metal_map.Sample(sampleState, texCoord).r;

    #ifdef CONSTRUCTION
    float4 gridTexture = grid_map.Sample(sampleState, texCoord * scaffoldTiling);
    #endif

    #ifdef DUAL_TEXTURE
    diffuse = lerp(diffuseMap2.Sample(sampleState, texCoord), diffuse, colour.a);
    normalTex = lerp(normalMap2.Sample(sampleState, texCoord), normalTex, colour.a);
    metalness = lerp(metalMap2.Sample(sampleState, texCoord).r, metalness, colour.a);
    #endif

    #ifdef COLOURING
    diffuse.rgb *= colour.rgb;
    #endif

    #ifdef DXT5NORMAL
    normalTex.x = normalTex.w;
    normalTex.y = normalTex.y;
    normalTex.xy = normalTex.xy * 2.0 - 1.0;
    normalTex.z = sqrt(1.0f - dot(normalTex.xy, normalTex.xy));
    #else
    normalTex.xyz = normalTex.xyz * 2.0 - 1.0;
    #endif

    float3x3 tbn = float3x3(normalize(tangent), normalize(binormal), normalize(normal));
    float3 worldNormal = normalize(mul(normalTex.xyz, tbn));

    #if defined(DOUBLESIDED) || defined(DOUBLESIDED2)
    if (!frontFace) worldNormal = -worldNormal;
    #endif

    #ifdef CONSTRUCTION
    float gridAmount = gridTexture.a;
    float finishingHeight = 1.0 - ((constructionState.x - 0.8) * 5.0);
    if (constructionState.x * 1.2 < construction.y)
    {
        clip(gridAmount - 0.7);
    }
    if (finishingHeight < construction.y)
    {
        gridAmount = 0;
    }
    gridTexture.a = 0.4;
    diffuse = lerp(diffuse, gridTexture, gridAmount);
    worldNormal = lerp(worldNormal, normal, gridAmount);
    #endif

    #ifdef TRANSPARENCY
    clip(normalTex.a - threshold);
    #endif

    #ifdef DUST
    #ifdef INTERIOR
    dustAmount.x = dustAmount.y;
    #endif
    float4 noise = dustNoise.Sample(sampleState, worldPos.xz * 0.002);
    float noiseEffect = 2.2 - noise.x * 6.0;
    float slopeEffect = saturate((worldNormal.y - 0.7 + dustAmount.z) * 4.0);
    float glossEffect = -diffuse.a * 6;
    float dust = slopeEffect + glossEffect + noiseEffect;
    dust = dust * 2.0 + dustAmount.x - 1.0;
    dust *= min(1.0, dustAmount.x * 8);
    diffuse.rgb = lerp(diffuse.rgb, dustColour.rgb, saturate(dust));
    worldNormal = lerp(worldNormal, normal, saturate(dust * 0.5));
    #endif

    diffuse.a *= glossMult;
    float absorb = 1.0 - diffuse.a;
    absorb *= 1.0 - metalness;
    #ifdef INTERIOR
    makeWet(diffuse, 0.0f, absorb, waterHeightRel - worldPos.y, 0.5);
    #else
    makeWet(diffuse, wetness, absorb, waterHeightRel - worldPos.y, 0.5);
    #endif

    diffuse.rgb = lerp(diffuse.rgb, coloroverride.rgb, coloroverride.a);

    INITIALISE_OUTPUT(buffer);
    writeAlbedo(buffer, diffuse.rgb, fragCoord.xy);
    writeMetalness(buffer, metalness);
    writeGloss(buffer, diffuse.a);
    writeNormal(buffer, worldNormal);
    writeDepth(buffer, length(worldPos - cameraPos) / farClip);

    #ifdef TRANSPARENCY
    float translucency = 1.0 - (normalTex.a - threshold) / (1.0 - threshold);
    buffer.buf1.a = translucency * 0.5;
    #endif

    #ifdef EMISSIVE
    buffer.buf1.a = 0.5 + brightness * (1.0f - normalTex.a) * 0.5;
    #endif
}
