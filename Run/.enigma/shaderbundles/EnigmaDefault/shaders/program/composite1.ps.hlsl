/// Composite pass 1 — volumetric light + underwater effects post-processing
/// Reads colortex5.a (cloudLinearDepth/vlFactor) written by deferred1
/// Phase 5: Added refraction, underwater color attenuation, caustics, underwater VL
/// Reference: CR composite1.glsl, design R4.6.2, design Component 6 (R5.5)
///
/// Pipeline: deferred1 output -> composite (SSR) -> composite1 (VL + underwater) -> final
/// Output: colortex0 (scene + VL + underwater), colortex5.a (vlFactor preserved)
#include "../@engine/core/core.hlsl"
#include "../@engine/lib/math.hlsl"
#include "../include/settings.hlsl"
#include "../lib/common.hlsl"
#include "../lib/shadow.hlsl"
#include "../lib/noise.hlsl"
#include "../lib/volumetricLight.hlsl"

// Water material mask threshold (241/255 = 0.9451)
static const float WATER_MATERIAL_THRESHOLD = 240.0 / 255.0;

// Underwater color attenuation (CR style, base values)
static const float3 UNDERWATER_MULT_DAY = float3(0.80, 0.87, 0.97) * 0.85;

struct PSOutput
{
    float4 color0 : SV_Target0; // colortex0
    float4 color1 : SV_Target1; // colortex5 (mapped via RENDERTARGETS: 0,5)
};

// ============================================================================
// Water Refraction (isEyeInWater == 0, looking at water from above)
// ============================================================================

/// Compute refracted scene color by offsetting UV based on depth difference.
/// Validates that the offset pixel is still water via colortex4 material mask.
float3 ApplyWaterRefraction(float2 uv, float3 sceneColor, float depth)
{
#if WATER_REFRACTION_INTENSITY > 0
    // Check if current pixel is water (colortex4.r material mask)
    float matMask = colortex4.Sample(sampler1, uv).r;
    if (matMask < WATER_MATERIAL_THRESHOLD)
        return sceneColor;

    // Read water normal from colortex2 for refraction direction
    float3 waterNormal = colortex2.Sample(sampler1, uv).rgb;

    // UV offset proportional to normal XY perturbation and refraction intensity
    float  refrScale = WATER_REFRACTION_INTENSITY * 0.01;
    float2 uvOffset  = waterNormal.xy * refrScale * 0.02;

    // Validate offset UV: must still be water
    float2 refractedUV = uv + uvOffset;
    refractedUV        = clamp(refractedUV, 0.001, 0.999);

    float offsetMatMask = colortex4.Sample(sampler1, refractedUV).r;
    if (offsetMatMask < WATER_MATERIAL_THRESHOLD)
        return sceneColor; // Offset left water region, keep original

    // Resample scene at refracted UV (already linearized)
    float3 refractedColor = colortex0.Sample(sampler0, refractedUV).rgb;
    refractedColor        = pow(max(refractedColor, 0.0), 2.2);

    return refractedColor;
#else
    return sceneColor;
#endif
}

// ============================================================================
// Main Entry
// ============================================================================

PSOutput main(PSInput input)
{
    PSOutput output;

    /* RENDERTARGETS: 0,5 */

    // Read scene color from deferred/composite output
    float3 sceneColor = colortex0.Sample(sampler0, input.TexCoord).rgb;
    float  depth      = depthtex0.Sample(sampler1, input.TexCoord).r;

    // Water reflection is now computed inline in gbuffers_water (CR architecture).
    // colortex0 already contains water color with reflection blended in.
    // Future: opaque surface reflections (metals, ice) would be blended here from colortex7.

    // Gamma -> Linear: deferred1 outputs gamma-space color (sRGB textures, gamma lighting).
    // Must linearize before VL addition and tonemap (composite5 applies LinearToSRGB).
    // Ref: CR composite1.glsl -- color = pow(color, vec3(2.2))
    sceneColor = pow(max(sceneColor, 0.0), 2.2);

    // Read cloud linear depth from colortex5.a (written by deferred1)
    // vlFactor: 1.0 = no cloud (full VL), <1.0 = cloud present (reduced VL)
    float vlFactor = colortex5.Sample(sampler1, input.TexCoord).a;

    // Compute sunVisibility from SdotU (CR composite1.glsl:24)
    float SdotU         = dot(normalize(sunPosition), normalize(upPosition));
    float sunVisibility = clamp(SdotU + 0.0625, 0.0, 0.125) / 0.125;
    // Reconstruct world position
    float3 viewPos  = ReconstructViewPosition(input.TexCoord, depth, gbufferProjectionInverse, gbufferRendererInverse);
    float3 worldPos = mul(gbufferViewInverse, float4(viewPos, 1.0)).xyz;

    // View direction (camera-relative, world space)
    // [FIX] worldPos is ABSOLUTE in our engine -- subtract cameraPosition
    float3 nViewDir = normalize(worldPos - cameraPosition);

    // Light direction in world space
    float3 lightDirWorld = normalize(mul((float3x3)gbufferViewInverse, shadowLightPosition));
    // Up direction in world space
    float3 upDirWorld = normalize(mul((float3x3)gbufferViewInverse, upPosition));

    // Directional dot products (CR composite1.glsl:196-198)
    float VdotL = dot(nViewDir, lightDirWorld);
    float VdotU = dot(nViewDir, upDirWorld);

    // Screen-space dither for ray march offset
    float dither = InterleavedGradientNoiseForClouds(input.Position.xy);

    // Volumetric light via shadow map ray marching
    float3 vl = GetVolumetricLight(
        worldPos, cameraPosition, VdotL, VdotU, SdotU, dither, vlFactor,
        sunVisibility,
        shadowView, shadowProjection,
        shadowtex1, sampler1);

    // ====================================================================
    // Phase 5: Water Refraction (isEyeInWater == 0, above water surface)
    // ====================================================================
    if (isEyeInWater == EYE_IN_AIR)
    {
        sceneColor = ApplyWaterRefraction(input.TexCoord, sceneColor, depth);
    }

    // ====================================================================
    // Phase 5: Underwater Effects (isEyeInWater == 1)
    // CR applies these in gamma space before pow(2.2). We're in linear space
    // here, so we must linearize fog color to match deferred1's fogged terrain.
    // ====================================================================
    if (isEyeInWater == EYE_IN_WATER)
    {
        // Time-dependent underwater colors (night darker, day brighter)
        float  sunVis2        = sunVisibility * sunVisibility;
        float3 underwaterMult = UNDERWATER_MULT_DAY * lerp(UNDERWATER_NIGHT_MULT, 1.0, sunVis2);
        float3 waterFogColor  = lerp(WATER_FOG_COLOR_NIGHT, WATER_FOG_COLOR_DAY, sunVis2);

        // Linearize fog color to match scene (already linear after pow(2.2) on line 88)
        waterFogColor = pow(max(waterFogColor, 0.0), 2.2);

        // [STEP 1] Sky pixel replacement BEFORE attenuation
        // Sky/unloaded chunks (depth=1.0) get fog color, then underwaterMult like terrain
        if (depth >= 0.9999)
        {
            sceneColor = waterFogColor;
        }

        // [STEP 2] Color attenuation -- applies to terrain AND sky uniformly
        sceneColor *= underwaterMult;

        // [STEP 3] VL attenuation -- underwater light filtering
        float3 uwMult071 = underwaterMult * 0.71;
        float3 vlMult    = uwMult071 * uwMult071; // pow2 per-component
        vl               *= vlMult;

        // NOTE: Caustics (shadowcolor0) and underwater VL (shadowcolor1) are NOT
        // sampled here. In CR architecture, shadowcolor is sampled inside the VL
        // ray march using shadow-space UVs (shadowPosition.xy), not screen UVs.
        // TODO: Integrate shadowcolor1 sampling into GetVolumetricLight() ray march
        //       for proper colored underwater light shafts (CR volumetricLight.glsl:185).
    }

    // Additive blend: VL adds light to scene (HDR, no clamping)
    // Tonemap in composite5 will handle HDR -> LDR conversion
    sceneColor += vl;

    output.color0 = float4(sceneColor, 1.0);
    output.color1 = float4(0.0, 0.0, 0.0, vlFactor);
    return output;
}
