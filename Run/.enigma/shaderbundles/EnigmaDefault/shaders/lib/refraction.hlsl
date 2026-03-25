/// Water surface refraction library (CR composite1.glsl DoRefraction port)
/// Noise-based UV warping with depth validation for artifact-free refraction.
///
/// Architecture:
///   composite1 calls DoWaterRefraction() when isEyeInWater == 0 (above water).
///   Uses customImage4 (noisetex) sampled at world-space coords for coherent distortion.
///   Three-stage validation: material check -> depth attenuation -> offset validation.
///
/// Dependencies:
///   - core.hlsl (colortex0/4, depthtex0/1, customImage4, samplers, matrices, uniforms)
///   - include/settings.hlsl (WATER_REFRACTION_INTENSITY, WATER_MATERIAL_THRESHOLD)
///
/// Reference: ComplementaryReimagined lib/materials/materialMethods/refraction.glsl

#ifndef LIB_REFRACTION_HLSL
#define LIB_REFRACTION_HLSL

// Water material mask threshold (241/255 = 0.9451)
// Pixels with colortex4.r >= this value are considered water
static const float WATER_MATERIAL_THRESHOLD = 240.0 / 255.0;

// ============================================================================
// Depth Linearization
// ============================================================================

/// Approximate linear view distance from non-linear depth buffer value.
/// Uses projection matrix elements to avoid explicit near/far uniforms.
/// DX12 standard depth [0,1]: z_linear = P[3][2] / (depth - P[2][2])
/// Reference: CR refraction.glsl GetApproxDistance (adapted from near*far/(far-depth*far))
float GetApproxDistance(float depth)
{
    return gbufferProjection[3][2] / (depth - gbufferProjection[2][2]);
}

// ============================================================================
// Water Surface Refraction (above water, looking down)
// ============================================================================

/// CR-style noise-based water refraction with three-stage validation.
/// Offsets screen UV using world-space noise, attenuated by depth difference
/// and view distance. Validates that offset pixels remain water to prevent
/// refraction bleeding into non-water regions.
///
/// @param uv         Screen-space texture coordinate
/// @param sceneColor Current scene color (linear space, post pow(2.2))
/// @param z0         depthtex0 value (includes water surface)
/// @param z1         depthtex1 value (opaque geometry only)
/// @param viewPos    View-space position of current fragment
/// @param lViewPos   Linear view distance (length(viewPos))
/// @return           Refracted scene color (linear space)
float3 DoWaterRefraction(float2 uv, float3     sceneColor, float z0, float z1,
                         float3 viewPos, float lViewPos)
{
#if WATER_REFRACTION_INTENSITY > 0
    // [Stage 1: Prep] Check if current pixel is water via material mask
    float matMask = colortex4.Sample(sampler1, uv).r;
    if (matMask < WATER_MATERIAL_THRESHOLD)
        return sceneColor;

    // FOV compensation: wider FOV = smaller refraction offset
    float fovScale = gbufferProjection[1][1];

    // World position for noise sampling (Z-up engine coordinate mapping)
    // CR: worldPos.xz * 0.02 + worldPos.y * 0.01 -> engine: .xy * 0.02 + .z * 0.01
    float3 worldPos   = mul(gbufferViewInverse, float4(viewPos, 1.0)).xyz;
    float2 noiseCoord = worldPos.xy * 0.02 + worldPos.z * 0.01
        + 0.01 * frameTimeCounter;

    // Sample noise texture (customImage4 = CR noisetex, RB channels)
    float2 refractNoise = customImage4.SampleLevel(sampler0, noiseCoord, 0).rb - 0.5;

    // Scale by intensity, FOV, and inverse distance (farther = less refraction)
    // CR: WATER_REFRACTION_INTENSITY * fovScale / (3.0 + lViewPos)
    refractNoise *= (WATER_REFRACTION_INTENSITY * 0.01) * fovScale / (3.0 + lViewPos);
    refractNoise *= 0.015; // Reimagined style base scale

    // [Stage 2: Check] Depth-based attenuation: reduce refraction in shallow water
    // approxDif = linear distance between water surface (z0) and opaque geometry (z1)
    // If z0 == z1, depthtex1 may not properly exclude water — skip attenuation
    if (abs(z1 - z0) > 0.00001)
    {
        float approxDif = GetApproxDistance(z1) - GetApproxDistance(z0);
        refractNoise    *= clamp(approxDif, 0.0, 1.0);
    }

    float2 refractCoord = clamp(uv + refractNoise, 0.001, 0.999);

    // Validate: offset pixel must still be water
    float offsetMatMask = colortex4.Sample(sampler1, refractCoord).r;
    if (offsetMatMask < WATER_MATERIAL_THRESHOLD)
        return sceneColor;

    // [Stage 3: Sample] Final refracted color
    refractCoord          = clamp(uv + refractNoise, 0.001, 0.999);
    float3 refractedColor = colortex0.SampleLevel(linearClampSampler, refractCoord, 0).rgb;
    refractedColor        = pow(max(refractedColor, 0.0), 2.2); // Linearize (colortex0 is gamma)

    return refractedColor;
#else
    return sceneColor;
#endif
}

#endif // LIB_REFRACTION_HLSL
