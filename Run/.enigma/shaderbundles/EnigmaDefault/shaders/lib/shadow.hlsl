/// [NEW] Shadow calculation utilities for EnigmaDefault ShaderBundle
/// Reference: miniature-shader/shaders/common/getShadowDistortion.glsl
/// Reference: miniature-shader/shaders/common/getLightStrength.fsh

#ifndef LIB_SHADOW_HLSL
#define LIB_SHADOW_HLSL

#include "../include/settings.hlsl"
#include "common.hlsl"

//============================================================================//
// Shadow Distortion
//============================================================================//

/// Distort shadow clip position to improve near-camera quality
/// This compresses the shadow map near the center for higher resolution
float3 GetShadowDistortion(float3 shadowClipPos)
{
    shadowClipPos.xy /= 0.8 * abs(shadowClipPos.xy) + 0.2;
    return shadowClipPos;
}

//============================================================================//
// Coordinate Transforms
//============================================================================//

/// Convert clip space [-1,1] to UV space [0,1]
float3 ClipToUV(float4 clipPos)
{
    float3 ndc  = clipPos.xyz / clipPos.w;
    float3 clip = float3(ndc.xy * 0.5 + 0.5, ndc.z);
    clip.y      = (1 - clip.y);
    return clip;
}

/// Transform world position to shadow UV coordinates
/// [FIX] Must match shadow.vs.hlsl transform chain: shadowView → gbufferRenderer → shadowProjection
float3 WorldToShadowUV(float3 worldPos, float4x4 shadowView, float4x4 shadowProj)
{
    float4 shadowViewPos   = mul(shadowView, float4(worldPos, 1.0));
    float4 shadowRenderPos = mul(gbufferRenderer, shadowViewPos); // [FIX] Add gbufferRenderer transform
    float4 shadowClipPos   = mul(shadowProj, shadowRenderPos);

    // Apply distortion for better near-camera quality
    shadowClipPos.xyz = GetShadowDistortion(shadowClipPos.xyz);

    return ClipToUV(shadowClipPos);
}

//============================================================================//
// Shadow Sampling
//============================================================================//

/// Check if UV is within valid shadow map bounds
bool IsValidShadowUV(float3 shadowUV)
{
    return shadowUV.x > 0.0 && shadowUV.x < 1.0 &&
        shadowUV.y > 0.0 && shadowUV.y < 1.0 &&
        shadowUV.z < 1.0;
}

/// Sample shadow map with distance fade
/// Returns shadow factor: 1.0 = fully lit, 0.0 = fully shadowed
float SampleShadowMap(float3 shadowUV, float3 worldPos, Texture2D shadowTex, SamplerState samp)
{
    float posDistSq = SquaredLength(worldPos);

    // Early out: beyond shadow distance
    if (posDistSq >= SHADOW_MAX_DIST_SQUARED)
        return 1.0;

    // Early out: outside shadow map bounds
    if (!IsValidShadowUV(shadowUV))
        return 1.0;

    // Distance-based fade (smooth transition at shadow boundary)
    float shadowFade = 1.0 - posDistSq * INV_SHADOW_MAX_DIST_SQUARED;

    // Sample shadow depth
    float shadowDepth = shadowTex.Sample(samp, shadowUV.xy).r;

    // Compare depths: if shadowUV.z > shadowDepth, pixel is in shadow
    // shadowUV.z = current pixel depth in light space
    // shadowDepth = closest depth stored in shadow map
    // When shadowUV.z > shadowDepth: something is closer to light → in shadow (0.0)
    // When shadowUV.z <= shadowDepth: nothing blocking → lit (1.0)
    float bias         = 0.001;
    float shadowFactor = (shadowUV.z - bias > shadowDepth) ? 0.0 : 1.0;

    // Apply fade: lerp from fully lit (1.0) to shadow factor at distance boundary
    return lerp(1.0, shadowFactor, shadowFade);
}

/// Simplified shadow sampling (uses engine shadowtex1 macro)
float SampleShadow(float3 worldPos, float4x4 shadowView, float4x4 shadowProj, SamplerState samp)
{
    float3 shadowUV = WorldToShadowUV(worldPos, shadowView, shadowProj);
    return SampleShadowMap(shadowUV, worldPos, shadowtex1, samp);
}

//============================================================================//
// Pixelated Shadow (Optional)
//============================================================================//

/// Apply pixelation to world position for stylized shadows
float3 PixelateShadowPos(float3 worldPos, float pixelSize)
{
    if (pixelSize <= 0.0)
        return worldPos;
    return Bandify(worldPos, pixelSize);
}

#endif // LIB_SHADOW_HLSL
