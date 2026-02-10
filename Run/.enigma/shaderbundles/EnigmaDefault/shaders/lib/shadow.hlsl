/// Shadow calculation utilities for EnigmaDefault ShaderBundle
/// Three-layer bias system based on ComplementaryReimagined
/// Ref: ComplementaryReimagined mainLighting.glsl, shadowSampling.glsl, shadow.glsl

#ifndef LIB_SHADOW_HLSL
#define LIB_SHADOW_HLSL

#include "../include/settings.hlsl"
#include "common.hlsl"

//============================================================================//
// Layer 1: Shadow Map Distortion
// Non-linear XY distortion + Z compression for depth precision
//============================================================================//

static const float shadowMapBias = 1.0 - 25.6 / SHADOW_DISTANCE;

/// Distort shadow clip position: center gets more resolution, Z compressed to 20%
/// Must match shadow.vs.hlsl
float3 GetShadowDistortion(float3 shadowClipPos)
{
    float lVertexPos    = length(shadowClipPos.xy);
    float distortFactor = lVertexPos * shadowMapBias + (1.0 - shadowMapBias);
    shadowClipPos.xy    *= 1.0 / distortFactor;
    shadowClipPos.z     *= 0.2;
    return shadowClipPos;
}

//============================================================================//
// Layer 2: Normal-Based World-Space Bias
// Offsets sampling position along surface normal to prevent acne
//============================================================================//

/// @param worldPos  World space position (relative to camera)
/// @param normal    World space surface normal (normalized)
/// @param lightDir  World space light direction (normalized, toward light)
float3 GetNormalShadowBias(float3 worldPos, float3 normal, float3 lightDir)
{
    float NdotL        = saturate(dot(normal, lightDir));
    float distSq       = dot(worldPos, worldPos);
    float distanceBias = 0.12 + 0.0008 * pow(distSq, 0.75);

    // Angle-adaptive: bias doubles when surface is parallel to light (NdotL → 0)
    return normal * distanceBias * (2.0 - 0.95 * NdotL);
}

//============================================================================//
// Coordinate Transforms
//============================================================================//

/// Clip space [-1,1] → UV space [0,1], Y flipped for D3D
float3 ClipToUV(float4 clipPos)
{
    float3 ndc = clipPos.xyz / clipPos.w;
    return float3(ndc.x * 0.5 + 0.5, -ndc.y * 0.5 + 0.5, ndc.z);
}

/// World position → shadow UV via: shadowView → gbufferRenderer → shadowProj → distortion
float3 WorldToShadowUV(float3 worldPos, float4x4 shadowView, float4x4 shadowProj)
{
    float4 shadowViewPos   = mul(shadowView, float4(worldPos, 1.0));
    float4 shadowRenderPos = mul(gbufferRenderer, shadowViewPos);
    float4 shadowClipPos   = mul(shadowProj, shadowRenderPos);
    shadowClipPos.xyz      = GetShadowDistortion(shadowClipPos.xyz);
    return ClipToUV(shadowClipPos);
}

//============================================================================//
// Pixelated Shadow (Optional)
// Ref: miniature-shader getLightStrength.fsh:12-16
//============================================================================//

float3 PixelateShadowPos(float3 worldPos, float pixelSize)
{
    if (pixelSize <= 0.0)
        return worldPos;
    return Bandify(worldPos, pixelSize);
}

//============================================================================//
// Shadow Sampling
//============================================================================//

bool IsValidShadowUV(float3 shadowUV)
{
    return shadowUV.x > 0.0 && shadowUV.x < 1.0 &&
        shadowUV.y > 0.0 && shadowUV.y < 1.0 &&
        shadowUV.z < 1.0;
}

/// Sample shadow map with soft depth comparison and distance fade
/// Ref: miniature-shader getLightStrength.fsh — continuous depth-based transition
/// @param shadowProjZ  shadowProjection._m22 (Z scale of shadow projection)
/// Returns: 1.0 = fully lit, 0.0 = fully shadowed
float SampleShadowMap(float3 shadowUV, float3 worldPos, float shadowProjZ, Texture2D shadowTex, SamplerState samp)
{
    float posDistSq = SquaredLength(worldPos);
    if (posDistSq >= SHADOW_MAX_DIST_SQUARED)
        return 1.0;
    if (!IsValidShadowUV(shadowUV))
        return 1.0;

    float shadowFade  = 1.0 - posDistSq * INV_SHADOW_MAX_DIST_SQUARED;
    float shadowDepth = shadowTex.Sample(samp, shadowUV.xy).r;

    // Soft shadow: depth difference → continuous [0,1] shadow amount
    // 0.2 = Z compression factor from GetShadowDistortion
    // 3.0 = transition sharpness (matches miniature-shader)
    float shadowAmount = saturate(3.0 * (shadowUV.z - shadowDepth) / (shadowProjZ * 0.2));

    return 1.0 - shadowFade * shadowAmount;
}

/// Preferred entry point: SHADOW_PIXEL + Layer 2 (normal offset) + soft depth comparison
/// Ref: miniature-shader getLightStrength.fsh (pixelation + shadow sampling)
float SampleShadowWithBias(float3    worldPos, float3        normal, float3 lightDir,
                           float4x4  shadowView, float4x4    shadowProj,
                           Texture2D shadowTex, SamplerState samp)
{
    // SHADOW_PIXEL: pixelate world position for stylized shadows
    // Ref: miniature-shader getLightStrength.fsh:12-16
    float3 shadowWorldPos = PixelateShadowPos(worldPos, SHADOW_PIXEL);

    float3 biasedWorldPos = shadowWorldPos + GetNormalShadowBias(shadowWorldPos, normal, lightDir);
    float3 shadowUV       = WorldToShadowUV(biasedWorldPos, shadowView, shadowProj);
    float  shadowProjZ    = shadowProj._m22;
    return SampleShadowMap(shadowUV, worldPos, shadowProjZ, shadowTex, samp);
}

/// Legacy: simplified shadow sampling without normal bias
float SampleShadow(float3 worldPos, float4x4 shadowView, float4x4 shadowProj, SamplerState samp)
{
    float3 shadowWorldPos = PixelateShadowPos(worldPos, SHADOW_PIXEL);
    float3 shadowUV       = WorldToShadowUV(shadowWorldPos, shadowView, shadowProj);
    float  shadowProjZ    = shadowProj._m22;
    return SampleShadowMap(shadowUV, worldPos, shadowProjZ, shadowtex1, samp);
}

#endif // LIB_SHADOW_HLSL
