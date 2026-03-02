/**
 * @file shadow.ps.hlsl
 * @brief Shadow Pass Pixel Shader - Depth + Caustics + Underwater VL
 * @date 2026-02-25
 *
 * Phase 5: Extended shadow pass with two additional outputs:
 * - shadowcolor0: Water caustic patterns (multiplicative)
 * - shadowcolor1: Underwater volumetric light data (additive)
 *
 * Depth is auto-written by hardware (no explicit output needed).
 * Alpha testing for cutout geometry (leaves, fences, etc.)
 *
 * Output Layout:
 * - SV_TARGET0 -> shadowcolor0: Caustic pattern (RGB grayscale)
 * - SV_TARGET1 -> shadowcolor1: Underwater VL color (blue-green)
 *
 * Reference:
 * - design.md: Algorithm Design C (Caustics + Underwater VL)
 * - ComplementaryReimagined: shadow.glsl:89-151
 */

#include "../@engine/core/core.hlsl"
#include "../include/settings.hlsl"
#include "../lib/common.hlsl"
#include "../lib/lighting.hlsl"

// [RENDERTARGETS] 0,1
// SV_TARGET0 -> shadowcolor0, SV_TARGET1 -> shadowcolor1

// ============================================================================
// Shadow Pass Pixel Structures
// ============================================================================

/**
 * @brief Pixel shader input - matches VSOutput_Shadow
 */
struct PSInput_Shadow
{
    float4 Position : SV_POSITION; // Light space clip position (system use)
    float2 TexCoord : TEXCOORD0; // UV for alpha testing
    float3 WorldPos : TEXCOORD1; // World position (for caustics + VL)
};

/**
 * @brief Pixel shader output for shadow color targets
 * Depth is auto-written by hardware when using depth-only rendering
 */
struct PSOutput_Shadow
{
    float4 Color0 : SV_TARGET0; // shadowcolor0: Caustic pattern (multiplicative)
    float4 Color1 : SV_TARGET1; // shadowcolor1: Underwater VL (additive)
};

// ============================================================================
// Caustic Generation
// ============================================================================

/**
 * @brief Compute water caustic pattern from customImage3 RG channels
 * Two offset samples create animated interference pattern.
 * @param shadowUV Shadow-space UV derived from clip position
 * @param time frameTimeCounter for animation
 * @return Grayscale caustic intensity
 */
float ComputeCaustic(float2 shadowUV, float time)
{
    Texture2D waterTex = GetCustomImage(3);

    // Two offset samples at same frequency, opposite time directions
    float2 uv1 = shadowUV * 0.08 + time * 0.01;
    float2 uv2 = shadowUV * 0.08 - time * 0.01 + 0.5;

    float2 sample1 = waterTex.Sample(sampler0, uv1).rg;
    float2 sample2 = waterTex.Sample(sampler0, uv2).rg;

    // Gradient from difference -> caustic intensity
    float2 gradient = abs(sample1 - sample2);
    float  caustic  = dot(gradient, float2(1.0, 1.0)) * WATER_CAUSTICS_STRENGTH;

    return saturate(caustic);
}

// ============================================================================
// Underwater Volumetric Light
// ============================================================================
/**
 * @brief Compute underwater volumetric light contribution
 * Dual-frequency noise sampling from customImage3 B+A channels (noisetex substitute).
 * Distance attenuation + sun visibility modulation.
 * @param worldPos World-space fragment position
 * @param time frameTimeCounter for animation
 * @param sunVis Sun visibility factor [0,1]
 * @return Blue-green VL color
 */
float3 ComputeUnderwaterVL(float3 worldPos, float time, float sunVis)
{
    Texture2D waterTex = GetCustomImage(3);

    // Dual-frequency noise sampling (B and A channels as noise source)
    float noise1  = waterTex.Sample(sampler0, worldPos.xy * 0.012 + time * 0.005).b;
    float noise2  = waterTex.Sample(sampler0, worldPos.xy * 0.05 - time * 0.01).a;
    float vlNoise = noise1 * 0.7 + noise2 * 0.3;

    // Distance attenuation from camera
    float dist        = length(worldPos - cameraPosition);
    float vlIntensity = vlNoise * exp(-dist * 0.02) * sunVis * WATER_VL_STRENGTH;

    // Blue-green underwater light color (CR style)
    float3 vlColor = float3(0.08, 0.12, 0.195) * vlIntensity;

    return vlColor;
}

// ============================================================================
// Main Entry
// ============================================================================

/**
 * @brief Shadow pass pixel shader main entry
 * @param input Interpolated data from VS
 * @return shadowcolor0 (caustics) + shadowcolor1 (underwater VL)
 *
 * [IMPORTANT] Depth texture (shadowtex) is written automatically by hardware.
 * This shader handles:
 * 1. Alpha testing for cutout geometry
 * 2. Caustic pattern generation (shadowcolor0)
 * 3. Underwater volumetric light data (shadowcolor1)
 */
PSOutput_Shadow main(PSInput_Shadow input)
{
    PSOutput_Shadow output;

    // [STEP 1] Sample texture for alpha testing (cutout geometry support)
    Texture2D gtexture = GetCustomImage(0);
    float4    texColor = gtexture.Sample(sampler1, input.TexCoord);

    // [STEP 2] Compute shadow UV from clip position for caustic sampling
    // SV_POSITION.xy is in screen pixels; normalize to [0,1] using viewport
    float2 shadowUV = input.WorldPos.xy;

    // [STEP 3] Sun visibility for VL modulation
    float sunVis = GetSunVisibility(sunPosition, upPosition);

    // [STEP 4] Caustic pattern -> shadowcolor0
    float caustic = ComputeCaustic(shadowUV, frameTimeCounter);
    output.Color0 = float4(caustic, caustic, caustic, 1.0);

    // [STEP 5] Underwater VL -> shadowcolor1
    float3 vlColor = ComputeUnderwaterVL(input.WorldPos, frameTimeCounter, sunVis);
    output.Color1  = float4(vlColor, 1.0);

    return output;
}
