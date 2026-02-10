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
// Shadow Sampling
//============================================================================//

bool IsValidShadowUV(float3 shadowUV)
{
    return shadowUV.x > 0.0 && shadowUV.x < 1.0 &&
        shadowUV.y > 0.0 && shadowUV.y < 1.0 &&
        shadowUV.z < 1.0;
}

/// Sample shadow map with Layer 3 residual Z-bias and distance fade
/// Returns: 1.0 = fully lit, 0.0 = fully shadowed
float SampleShadowMap(float3 shadowUV, float3 worldPos, Texture2D shadowTex, SamplerState samp)
{
    float posDistSq = SquaredLength(worldPos);
    if (posDistSq >= SHADOW_MAX_DIST_SQUARED)
        return 1.0;
    if (!IsValidShadowUV(shadowUV))
        return 1.0;

    float shadowFade   = 1.0 - posDistSq * INV_SHADOW_MAX_DIST_SQUARED;
    float shadowDepth  = shadowTex.Sample(samp, shadowUV.xy).r;
    float shadowFactor = (shadowUV.z - 0.0004 > shadowDepth) ? 0.0 : 1.0;

    return lerp(1.0, shadowFactor, shadowFade);
}

/// Preferred entry point: Layer 2 (normal offset) + Layer 3 (residual Z-bias)
float SampleShadowWithBias(float3    worldPos, float3        normal, float3 lightDir,
                           float4x4  shadowView, float4x4    shadowProj,
                           Texture2D shadowTex, SamplerState samp)
{
    float3 biasedWorldPos = worldPos + GetNormalShadowBias(worldPos, normal, lightDir);
    float3 shadowUV       = WorldToShadowUV(biasedWorldPos, shadowView, shadowProj);
    return SampleShadowMap(shadowUV, worldPos, shadowTex, samp);
}

/// Legacy: simplified shadow sampling without normal bias
float SampleShadow(float3 worldPos, float4x4 shadowView, float4x4 shadowProj, SamplerState samp)
{
    float3 shadowUV = WorldToShadowUV(worldPos, shadowView, shadowProj);
    return SampleShadowMap(shadowUV, worldPos, shadowtex1, samp);
}

//============================================================================//
// Pixelated Shadow (Optional)
//============================================================================//

float3 PixelateShadowPos(float3 worldPos, float pixelSize)
{
    if (pixelSize <= 0.0)
        return worldPos;
    return Bandify(worldPos, pixelSize);
}

#endif // LIB_SHADOW_HLSL
