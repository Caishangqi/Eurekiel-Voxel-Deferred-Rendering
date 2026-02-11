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

//============================================================================//
// PCF Shadow Filtering
// Ref: ComplementaryReimagined shadowSampling.glsl
//============================================================================//

/// PCF kernel offset based on SHADOW_SMOOTHING level
/// Ref: ComplementaryReimagined mainLighting.glsl:156-164
static const float SHADOW_PCF_OFFSET =
#if SHADOW_SMOOTHING == 1
    0.0003;
#elif SHADOW_SMOOTHING == 2
    0.0005;
#elif SHADOW_SMOOTHING == 3
    0.00075;
#else // SHADOW_SMOOTHING == 4
    0.00098;
#endif

/// PCF sample count based on SHADOW_QUALITY level
/// Each sample produces 2 taps (positive + negative offset), so effective taps = samples * 2
/// Ref: ComplementaryReimagined mainLighting.glsl:258-268
static const int SHADOW_PCF_SAMPLES =
#if SHADOW_QUALITY <= 0
    0;
#elif SHADOW_QUALITY == 1
    1; // 2 effective taps
#elif SHADOW_QUALITY == 2
    2; // 4 effective taps
#elif SHADOW_QUALITY == 3
    3; // 6 effective taps
#elif SHADOW_QUALITY == 4
    4; // 8 effective taps
#else // SHADOW_QUALITY == 5
    8; // 16 effective taps
#endif

/// Interleaved Gradient Noise — screen-space pseudo-random value
/// Produces well-distributed noise per pixel, ideal for PCF jittering
/// Ref: ComplementaryReimagined shadowSampling.glsl InterleavedGradientNoiseForShadows()
/// @param screenPos SV_Position.xy (pixel coordinates)
float InterleavedGradientNoise(float2 screenPos)
{
    float n = 52.9829189 * frac(0.06711056 * screenPos.x + 0.00583715 * screenPos.y);
    return frac(n);
    // TODO: When frameCounter uniform is available, add temporal jitter:
    // return frac(n + 1.6180339887 * fmod(frameCounter, 3600.0));
}

/// Convert scalar noise to circular 2D offset for PCF sampling
/// Ref: ComplementaryReimagined shadowSampling.glsl offsetDist()
/// @param x  Noise value (gradientNoise + sampleIndex)
/// @param s  Total sample count (for normalization)
float2 OffsetDist(float x, int s)
{
    float angle = frac(x * 2.427) * 6.28318530718; // Convert to angle [0, 2π]
    return float2(cos(angle), sin(angle)) * 1.4 * x / s;
}

/// Basic 4-tap cross-pattern shadow filter (SHADOW_QUALITY == 0)
/// Ref: ComplementaryReimagined shadowSampling.glsl SampleBasicFilteredShadow()
float SampleBasicFilteredShadow(float3 shadowUV, float        offset, float3          worldPos,
                                float  shadowProjZ, Texture2D shadowTex, SamplerState samp)
{
    static const float2 offsets[4] = {
        float2(1.0, 0.0),
        float2(0.0, 1.0),
        float2(-1.0, 0.0),
        float2(0.0, -1.0)
    };

    float shadow = 0.0;
    [unroll]
    for (int i = 0; i < 4; i++)
    {
        float3 offsetUV = float3(shadowUV.xy + offset * offsets[i], shadowUV.z);
        shadow          += SampleShadowMap(offsetUV, worldPos, shadowProjZ, shadowTex, samp);
    }
    return shadow * 0.25;
}

/// Circular PCF shadow filter with IGN jittering
/// Dual-direction sampling: each iteration samples +offset and -offset
/// Ref: ComplementaryReimagined shadowSampling.glsl SampleTAAFilteredShadow()
/// @param screenPos SV_Position.xy for noise generation
float SamplePCFFilteredShadow(float3    shadowUV, float         offset, int     shadowSamples,
                              float2    screenPos, float3       worldPos, float shadowProjZ,
                              Texture2D shadowTex, SamplerState samp)
{
    float shadow        = 0.0;
    float gradientNoise = InterleavedGradientNoise(screenPos);

    for (int i = 0; i < shadowSamples; i++)
    {
        float2 sampleOffset = OffsetDist(gradientNoise + i, shadowSamples) * offset;

        // Positive direction
        float3 uvPos = float3(shadowUV.xy + sampleOffset, shadowUV.z);
        shadow       += SampleShadowMap(uvPos, worldPos, shadowProjZ, shadowTex, samp);

        // Negative direction (doubles coverage without extra noise)
        float3 uvNeg = float3(shadowUV.xy - sampleOffset, shadowUV.z);
        shadow       += SampleShadowMap(uvNeg, worldPos, shadowProjZ, shadowTex, samp);
    }

    return shadow / (shadowSamples * 2.0);
}

//============================================================================//
// Shadow Entry Points
//============================================================================//

/// Main entry point: normal bias + PCF filtering
/// Ref: ComplementaryReimagined mainLighting.glsl GetShadow() dispatch
/// @param screenPos SV_Position.xy (required for PCF noise, ignored when SHADOW_QUALITY < 0)
float SampleShadowWithBias(float3    worldPos, float3        normal, float3 lightDir,
                           float4x4  shadowView, float4x4    shadowProj,
                           Texture2D shadowTex, SamplerState samp,
                           float2    screenPos)
{
    // SHADOW_PIXEL: pixelate world position for stylized shadows
    float3 shadowWorldPos = PixelateShadowPos(worldPos, SHADOW_PIXEL);

    float3 biasedWorldPos = shadowWorldPos + GetNormalShadowBias(shadowWorldPos, normal, lightDir);
    float3 shadowUV       = WorldToShadowUV(biasedWorldPos, shadowView, shadowProj);
    float  shadowProjZ    = shadowProj._m22;

    // Early out: outside shadow map or too far
    float posDistSq = SquaredLength(worldPos);
    if (posDistSq >= SHADOW_MAX_DIST_SQUARED)
        return 1.0;
    if (!IsValidShadowUV(shadowUV))
        return 1.0;

    // --- Shadow edge fade zone ---
    // Ref: ComplementaryReimagined mainLighting.glsl:273-289
    // Smooth transition at shadow distance boundary to avoid hard cutoff
    float posDist        = sqrt(posDistSq);
    float shadowEdgeDist = SHADOW_DISTANCE - posDist; // distance to shadow boundary
    float edgeFade       = saturate(shadowEdgeDist / SHADOW_EDGE_FADE_RANGE);

#if SHADOW_QUALITY < 0
    // Hard shadows — single sample, no filtering
    float shadow = SampleShadowMap(shadowUV, worldPos, shadowProjZ, shadowTex, samp);
    return lerp(1.0, shadow, edgeFade);
#elif SHADOW_QUALITY == 0
    // Basic 4-tap cross filter
    float shadow = SampleBasicFilteredShadow(shadowUV, SHADOW_PCF_OFFSET, worldPos, shadowProjZ, shadowTex, samp);
    return lerp(1.0, shadow, edgeFade);
#else
    // Circular PCF with IGN jittering
    float offset = SHADOW_PCF_OFFSET;

    // Distance-adaptive PCF: scale offset with distance from camera
    // Far shadows have larger texels due to shadow map distortion,
    // so we widen the PCF kernel to compensate for the lower effective resolution
    float distRatio = posDist / SHADOW_DISTANCE; // [0, 1]
    offset          *= 1.0 + distRatio * SHADOW_PCF_DIST_SCALE;

    // Increase spread during rain for softer shadows
    // Ref: ComplementaryReimagined mainLighting.glsl:170
    offset *= 1.0 + rainStrength * 2.0;

    float shadow = SamplePCFFilteredShadow(shadowUV, offset, SHADOW_PCF_SAMPLES, screenPos,
                                           worldPos, shadowProjZ, shadowTex, samp);

    // Blend to fully lit at shadow distance boundary
    return lerp(1.0, shadow, edgeFade);
#endif
}

/// Legacy: simplified shadow sampling without normal bias (no PCF)
float SampleShadow(float3 worldPos, float4x4 shadowView, float4x4 shadowProj, SamplerState samp)
{
    float3 shadowWorldPos = PixelateShadowPos(worldPos, SHADOW_PIXEL);
    float3 shadowUV       = WorldToShadowUV(shadowWorldPos, shadowView, shadowProj);
    float  shadowProjZ    = shadowProj._m22;
    return SampleShadowMap(shadowUV, worldPos, shadowProjZ, shadowtex1, samp);
}

#endif // LIB_SHADOW_HLSL
