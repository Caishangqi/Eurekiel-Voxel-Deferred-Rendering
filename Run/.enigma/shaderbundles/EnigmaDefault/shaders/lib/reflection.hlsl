/**
 * @file reflection.hlsl
 * @brief Three-layer reflection coordinator: Mirrored Image + Procedural Sky + GGX + Blend
 *
 * Bridges the gap between SSR ray march (near-field) and sky fallback (far-field)
 * using CR's Method 2 mirrored image reflection as the middle layer.
 *
 * Architecture:
 *   Layer 1: SSR Ray March     (near, high quality, from ssr.hlsl)
 *   Layer 2: Mirrored Image    (mid, colortex0 reprojection, this file)
 *   Layer 3: Procedural Sky    (far/fallback, atmospheric gradient, this file)
 *
 * Dependencies:
 *   - @engine/core/core.hlsl   (textures, matrices, samplers, uniforms)
 *   - @engine/lib/math.hlsl    (ReconstructViewPosition)
 *   - include/settings.hlsl    (MIRROR_*, SKY_REFLECT_*)
 *   - lib/common.hlsl          (Luma, Pow2, Pow4)
 *   - lib/ssr.hlsl             (SSRResult struct)
 *
 * Reference: ComplementaryReimagined reflections.glsl:185-233 (Method 2)
 *            ComplementaryReimagined GetLowQualitySky (sky gradient)
 */

#ifndef LIB_REFLECTION_HLSL
#define LIB_REFLECTION_HLSL

// ============================================================================
// Reflection Result (three-layer blend output)
// ============================================================================

struct ReflectionResult
{
    float3 color; // Final blended reflection color
    float  alpha; // Overall reflection confidence [0,1]
};

// ============================================================================
// Layer 2: Mirrored Image Reflection (CR Method 2)
// ============================================================================

/// Project reflected direction to screen space and sample colortex0.
/// Provides mid-distance reflection where SSR runs out of steps but the
/// scene is still visible on screen from a different angle.
///
/// @param viewPos       View-space fragment position
/// @param nViewPos      Normalized view-to-fragment direction (view space)
/// @param reflectDirVS  Reflected direction in view space
/// @param lViewPos      Distance from camera to fragment
/// @param dither        Temporal jitter [0,1)
/// @return SSRResult with mirrored color and confidence alpha
SSRResult ComputeMirroredReflection(
    float3 viewPos,
    float3 nViewPos,
    float3 reflectDirVS,
    float  lViewPos,
    float  dither)
{
    SSRResult result;
    result.color = float3(0.0, 0.0, 0.0);
    result.alpha = 0.0;

    // Screen edge bound (CR: rEdge = 0.525)
    static const float SCREEN_EDGE = 0.525;

    // Project reflected point to clip space
    // CR: pos + reflect * (0.013 * viewPos.y) stretch for vertical parallax
    float3 mirrorViewPos = reflectDirVS + MIRROR_VERTICAL_STRETCH * viewPos;
    float4 renderPos     = mul(gbufferRenderer, float4(mirrorViewPos, 1.0));
    float4 clipPosR      = mul(gbufferProjection, float4(renderPos.xyz, 1.0));

    // Perspective divide -> screen UV
    if (clipPosR.w <= 0.0)
        return result;

    float3 ndcR    = clipPosR.xyz / clipPosR.w;
    float2 screenR = float2(ndcR.x * 0.5 + 0.5, -ndcR.y * 0.5 + 0.5);

    // Boundary check
    if (abs(screenR.x - 0.5) > SCREEN_EDGE || abs(screenR.y - 0.5) > SCREEN_EDGE)
        return result;

    // View angle consistency: reject if reflection points behind camera
    if (dot(nViewPos, reflectDirVS) > 0.45)
        return result;

    // Sample opaque depth to reject sky pixels (z >= 0.9997)
    float sceneDepthR = depthtex1.Sample(sampler1, screenR).r;
    if (sceneDepthR >= 0.9997)
        return result;

    // Sample reflected scene color
    result.color = colortex0.SampleLevel(sampler0, screenR, 0).rgb;

    // Edge fade: pow8 for smooth screen-border falloff (CR reflections.glsl:220)
    float2 cdist      = abs(screenR - 0.5) / SCREEN_EDGE;
    float2 cdist4     = cdist * cdist * cdist * cdist;
    float2 edgeFactor = 1.0 - cdist4 * cdist4;
    float  edgeAlpha  = edgeFactor.x * edgeFactor.y;

    // Distance fog: fade mirror reflection at far range
    // CR: exp(-3.0 * pow(dist/far, 1.5))
    float distRatio = lViewPos / far;
    float distFog   = exp(-MIRROR_FOG_DENSITY * pow(distRatio, MIRROR_DISTANCE_FOG_POWER));

    result.alpha = edgeAlpha * distFog;

    return result;
}

// ============================================================================
// Layer 3: Procedural Sky Reflection (simplified CR GetLowQualitySky)
// ============================================================================

/// Atmospheric sky gradient for reflection fallback.
/// Replaces the old flat GetReflectionFallback with a proper sky dome:
/// zenith-to-horizon gradient, sunset warmth, below-horizon darkening.
///
/// @param reflectDir     World-space reflection direction (normalized)
/// @param skyLightFactor Sky light influence [0,1] from lightmap
/// @param sunVis2        Sun visibility squared (0=night, 1=day)
/// @param noonFactor     Noon proximity (0=horizon/night, 1=noon)
/// @return Procedural sky reflection color
float3 ComputeSkyReflection(
    float3 reflectDir,
    float  skyLightFactor,
    float  sunVis2,
    float  noonFactor)
{
    // Reflection direction dot up (engine: +Z is up)
    float RVdotU = reflectDir.z;

    // Day/night zenith and horizon colors
    float3 upColor  = lerp(SKY_REFLECT_UP_NIGHT, SKY_REFLECT_UP_DAY, sunVis2);
    float3 midColor = lerp(SKY_REFLECT_MID_NIGHT, SKY_REFLECT_MID_DAY, sunVis2);

    // Zenith-to-horizon gradient: upward = zenith blue, horizontal = haze
    float  upBlend = saturate(RVdotU);
    float3 skyCol  = lerp(midColor, upColor, upBlend);

    // Sunset warmth overlay: strongest at horizon during sunrise/sunset
    float sunsetStrength = (1.0 - noonFactor) * sunVis2;
    float horizonMask    = 1.0 - abs(RVdotU); // strongest at horizon
    skyCol               = lerp(skyCol, SKY_REFLECT_SUNSET * 0.6, sunsetStrength * horizonMask * 0.5);

    // Below-horizon darkening: reflections pointing downward fade to dark
    float belowHorizon = saturate(-RVdotU * 2.0);
    skyCol             *= 1.0 - belowHorizon * 0.7;

    // Modulate by sky light (indoor/cave = no sky reflection)
    return skyCol * skyLightFactor;
}

// ============================================================================
// GGX Specular Highlight (sun/moon on water surface)
// ============================================================================

/// GGX microfacet specular for sun/moon highlight on water.
/// @param normal     Surface normal (world space)
/// @param viewDir    Camera-to-fragment direction (world space, normalized)
/// @param lightDir   Light direction toward surface (world space, normalized)
/// @param NdotL      dot(normal, lightDir), pre-clamped
/// @param smoothness Surface smoothness [0,1]
/// @return Specular intensity
float GGXSpecular(
    float3 normal,
    float3 viewDir,
    float3 lightDir,
    float  NdotL,
    float  smoothness)
{
    float roughness = 1.0 - smoothness;
    float alpha     = roughness * roughness;
    float alpha2    = alpha * alpha;

    float3 halfVec = normalize(lightDir - viewDir);
    float  NdotH   = max(dot(normal, halfVec), 0.0);
    float  NdotV   = max(dot(normal, -viewDir), 0.0);

    // GGX distribution
    float denom = NdotH * NdotH * (alpha2 - 1.0) + 1.0;
    float D     = alpha2 / (PI * denom * denom + 0.0001);

    // Schlick geometry approximation
    float k  = alpha * 0.5;
    float G1 = NdotV / (NdotV * (1.0 - k) + k + 0.0001);
    float G2 = NdotL / (NdotL * (1.0 - k) + k + 0.0001);

    return D * G1 * G2 * 0.25;
}

// ============================================================================
// Three-Layer Blend
// ============================================================================

/// Blend SSR, mirrored image, and procedural sky into final reflection.
/// Priority: SSR (highest) -> Mirror -> Sky (lowest).
/// Each layer's alpha acts as a coverage mask; uncovered regions fall through.
///
/// @param ssr            SSR ray march result (Layer 1)
/// @param mirror         Mirrored image result (Layer 2)
/// @param skyReflection  Procedural sky color (Layer 3, always full coverage)
/// @return Final blended reflection
ReflectionResult BlendReflectionLayers(
    SSRResult ssr,
    SSRResult mirror,
    float3    skyReflection)
{
    ReflectionResult result;

    // Start with sky as base (always available)
    float3 blended = skyReflection;

    // Layer mirror on top of sky (mirror.alpha = coverage)
    blended = lerp(blended, mirror.color, mirror.alpha);

    // Layer SSR on top (highest priority)
    blended = lerp(blended, ssr.color, ssr.alpha);

    // Overall alpha: at least sky is always present
    result.color = blended;
    result.alpha = saturate(max(ssr.alpha, max(mirror.alpha, 0.5)));

    return result;
}

#endif // LIB_REFLECTION_HLSL
