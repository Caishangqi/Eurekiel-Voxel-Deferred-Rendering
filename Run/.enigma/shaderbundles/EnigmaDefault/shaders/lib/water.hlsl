/**
 * @file water.hlsl
 * @brief Water effect library - Reimagined style surface rendering
 *
 * Port of ComplementaryReimagined water.glsl to HLSL.
 * Computes water color, transparency, normals, foam, and fresnel.
 * Does NOT include lighting — that happens in deferred1.
 *
 * Coordinate mapping (CR OpenGL Y-up -> Engine DX12 Z-up):
 *   CR worldPos.xz -> engine worldPos.xy (horizontal plane)
 *   CR worldPos.y  -> engine worldPos.z  (vertical axis)
 *
 * Dependencies:
 *   - lib/common.hlsl       (Pow2, Luma)
 *   - include/settings.hlsl (WATER_* parameters)
 *   - customImage3          (cloud-water.png, RG=normals, A=height)
 *   - sampler0              (wrap/repeat sampler for tiling)
 *
 * Requirements: R5.2
 */

#ifndef LIB_WATER_HLSL
#define LIB_WATER_HLSL

// Normal layer blend weights (CR defaults)
#define WATER_BUMP_MED   0.5
#define WATER_BUMP_SMALL 1.0
#define WATER_BUMP_BIG   0.3

// ============================================================================
// Water Result Structure
// ============================================================================

struct WaterResult
{
    float3 color; // Final water color (pre-lighting)
    float  alpha; // Water transparency
    float3 normalM; // Perturbed world-space normal
    float  smoothness; // Surface smoothness (0.5 for Reimagined)
    float  fresnel; // Fresnel coefficient [0,1]
    float  foam; // Foam intensity [0,1]
};

// ============================================================================
// Internal Helpers
// ============================================================================

/**
 * @brief Build TBN basis for water surface
 * Water is mostly horizontal (normal ~= (0,0,1) in Z-up engine).
 * For non-horizontal surfaces, derive tangent from world up.
 */
void BuildWaterTBN(float3 geoNormal, out float3 T, out float3 B)
{
    if (abs(geoNormal.z) > 0.999)
    {
        T = float3(1, 0, 0);
        B = float3(0, 1, 0);
    }
    else
    {
        T = normalize(cross(float3(0, 0, 1), geoNormal));
        B = cross(geoNormal, T);
    }
}

/**
 * @brief Sample multi-layer water normals from customImage3 RG channels
 * Three frequency layers: medium, small (high-freq detail), big (low-freq swell).
 * Reference: CR water.glsl lines 96-101
 *
 * @param waterPos  Scaled water texture coordinate
 * @param wind      Wind offset vector (animated)
 * @param geoFresnel Geometric fresnel for bumpiness attenuation at grazing angles
 * @return float2   Normal map XY perturbation
 */
float2 SampleWaterNormals(float2 waterPos, float2 wind, float geoFresnel)
{
    Texture2D waterTex = customImage3;

    float2 normalMed   = waterTex.Sample(sampler0, waterPos + wind).rg - 0.5;
    float2 normalSmall = waterTex.Sample(sampler0, waterPos * 4.0 - 2.0 * wind).rg - 0.5;
    float2 normalBig   = waterTex.Sample(sampler0, waterPos * 0.25 - 0.5 * wind).rg - 0.5;
    normalBig          += waterTex.Sample(sampler0, waterPos * 0.05 - 0.05 * wind).rg - 0.5;

    float2 normalXY = normalMed * WATER_BUMP_MED
        + normalSmall * WATER_BUMP_SMALL
        + normalBig * WATER_BUMP_BIG;

    // Bumpiness modulation: attenuate at grazing angles, scale by user setting
    float bumpScale = 6.0 * (1.0 - 0.7 * geoFresnel) * (WATER_BUMPINESS * 0.01);
    normalXY        *= bumpScale;

    // Final scale (CR: normalMap.xy *= 0.03 * lmCoordM.y + 0.01)
    // Simplified: use fixed scale since we lack per-fragment skylight here
    normalXY *= 0.035;

    return normalXY;
}

/**
 * @brief Apply parallax mapping to water texture coordinates
 * Shifts waterPos based on view angle for depth illusion.
 * Only active when WATER_MAT_QUALITY >= 3.
 * Reference: CR water.glsl lines 89-93
 *
 * @param waterPos  Input/output water texture coordinate
 * @param wind      Wind offset
 * @param viewDir   Fragment-to-camera direction (world space)
 */
void ApplyWaterParallax(inout float2 waterPos, float2 wind, float3 viewDir)
{
#if WATER_MAT_QUALITY >= 3
    Texture2D waterTex = customImage3;

    // Parallax offset: horizontal shift based on view angle
    // viewDir.xy = horizontal, viewDir.z = vertical (Z-up engine)
    float2 parallaxMult = -0.01 * viewDir.xy / max(abs(viewDir.z), 0.001);

    [unroll]
    for (int i = 0; i < 4; i++)
    {
        waterPos += parallaxMult * waterTex.SampleLevel(sampler0, waterPos - wind, 0).a;
        waterPos += parallaxMult * waterTex.SampleLevel(sampler0, waterPos * 0.25 - 0.5 * wind, 0).a;
    }
#endif
}

/**
 * @brief Compute rain ripple normal perturbation
 * Adds animated ripple pattern when raining.
 * Uses customImage3 at different frequency for ripple pattern.
 * Reference: CR water.glsl lines 107-119 (simplified)
 *
 * @param worldPosXY Horizontal world position
 * @param frameTime  Animation time
 * @param rainStrength Rain intensity [0,1]
 * @return float2    Ripple normal XY perturbation
 */
float2 GetRainRipples(float2 worldPosXY, float frameTime, float rainStrength)
{
    if (rainStrength < 0.001)
        return float2(0, 0);

    Texture2D waterTex = customImage3;

    float2 rippleWind = float2(frameTime, frameTime) * 0.015;
    float2 ripplePos  = floor(worldPosXY * 16.0) * 0.00625;

    float2 noise1 = waterTex.SampleLevel(sampler0, ripplePos + rippleWind, 0).rg;
    float2 noise2 = waterTex.SampleLevel(sampler0, ripplePos - rippleWind * 1.5, 0).rg;

    float2 ripple = (noise1 + noise2.yx - 1.0) * 0.02 * rainStrength;
    return ripple;
}

/**
 * @brief Compute foam intensity based on water color luminance
 * Brighter water color indicates shallower water -> more foam.
 * Reimagined style: uses squared color dot product as threshold.
 * Reference: CR water.glsl lines 204-228
 *
 * @param waterColor Water color after tinting
 * @param texColorSq Squared texture color (dot(colorPM, colorPM))
 * @return float     Foam intensity [0,1]
 */
float ComputeWaterFoam(float3 waterColor, float texColorSq)
{
#if WATER_FOAM_I > 0
    float foamThreshold = min(Pow2(texColorSq) * 1.6, 1.2);
    float foam          = Pow2(saturate((foamThreshold + 0.1) / max(foamThreshold, 0.01)));
    foam                *= WATER_FOAM_I * 0.01;
    return saturate(foam);
#else
    return 0.0;
#endif
}

// ============================================================================
// Main Water Processing Function
// ============================================================================

/**
 * @brief Compute complete water surface properties (Reimagined style)
 *
 * Called from gbuffers_water.ps.hlsl when mat == 32000.
 * Produces color, alpha, perturbed normal, smoothness, fresnel, and foam.
 * Does NOT apply lighting — deferred1 handles that via G-Buffer data.
 *
 * @param worldPos     World position of water fragment
 * @param viewDir      Normalized view direction, fragment-to-camera (world space)
 * @param geoNormal    Geometric normal (world space, typically (0,0,1) for water)
 * @param texColor     Sampled atlas texture color
 * @param vertexColor  Vertex color (includes MC biome tint)
 * @param rainStrength Rain intensity [0,1]
 * @param frameTime    Animation time (frameTimeCounter)
 * @return WaterResult Complete water surface data
 */
WaterResult ComputeWaterSurface(
    float3 worldPos,
    float3 viewDir,
    float3 geoNormal,
    float4 texColor,
    float4 vertexColor,
    float  rainStrength,
    float  frameTime)
{
    WaterResult result;

    // === Step 1: Water Color (Reimagined) ===
    // CR: color.rgb = pow2(colorP.rgb) * glColorM
    // glColorM = sqrt(glColor.rgb) * vec3(1.0, 0.85, 0.8)
    float3 colorPM  = float3(Pow2(texColor.r), Pow2(texColor.g), Pow2(texColor.b));
    float3 glColorM = sqrt(abs(vertexColor.rgb)) * float3(1.0, 0.85, 0.8);
    result.color    = colorPM * glColorM;

    // === Step 2: Water Alpha ===
    // CR: color.a = sqrt(color.a) with fog-based modulation
    result.alpha = sqrt(saturate(texColor.a * vertexColor.a));

    // Alpha multiplier from settings
#if WATER_ALPHA_MULT != 100
    result.alpha = pow(result.alpha, 100.0 / WATER_ALPHA_MULT);
#endif

    // === Step 3: Wind and Water Position ===
    // CR: rawWind = frameTimeCounter * WATER_SPEED_MULT * 0.018
    float  rawWind = frameTime * WATER_WAVE_SPEED * 0.018;
    float2 wind    = float2(0, -rawWind);

    // CR: waterPos = 0.032 * (worldPos.xz + worldPos.y * 2.0)
    // Engine Z-up: horizontal = XY, vertical = Z
    float2 waterPos = 0.032 * (worldPos.xy + worldPos.z * 2.0);

    // === Step 4: Parallax Mapping (WATER_MAT_QUALITY >= 3) ===
    ApplyWaterParallax(waterPos, wind, viewDir);

    // === Step 5: Normal Generation ===
    // Geometric fresnel for bumpiness attenuation
    float geoFresnel = saturate(1.0 + dot(geoNormal, -viewDir));

    // Multi-layer normal sampling from customImage3 RG
    float2 normalXY = SampleWaterNormals(waterPos, wind, geoFresnel);

    // Rain ripples overlay
    normalXY += GetRainRipples(worldPos.xy, frameTime, rainStrength);

    // Reconstruct Z from XY (unit normal constraint)
    float3 normalMap;
    normalMap.xy = normalXY;
    normalMap.z  = sqrt(max(1.0 - dot(normalXY, normalXY), 0.0));

    // Transform from tangent space to world space via TBN
    float3 T, B;
    BuildWaterTBN(geoNormal, T, B);
    result.normalM = normalize(T * normalMap.x + B * normalMap.y + geoNormal * normalMap.z);
    result.normalM = clamp(result.normalM, float3(-1, -1, -1), float3(1, 1, 1));

    // CR water.glsl:130-132: Fix normals pointing inside water surface
    // If the reflected view direction points below the surface, blend normal back toward flat
    // This prevents reflection angles exceeding 180 degrees at extreme perturbations
    float3 nViewPos     = -viewDir; // camera-to-fragment direction
    float3 reflectCheck = reflect(nViewPos, normalize(result.normalM));
    float  norMix       = pow(1.0 - max(dot(geoNormal, reflectCheck), 0.0), 8.0) * 0.5;
    result.normalM      = lerp(result.normalM, geoNormal, norMix);

    // === Step 6: Foam ===
    float texColorSq = dot(colorPM, colorPM);
    result.foam      = ComputeWaterFoam(result.color, texColorSq);

    // Blend foam into color (white-ish foam tint)
    if (result.foam > 0.001)
    {
        float3 foamColor = float3(0.9, 0.95, 1.05);
        result.color     = lerp(result.color, foamColor, result.foam);
        result.alpha     = lerp(result.alpha, 1.0, result.foam);
    }

    // === Step 7: Fresnel (from perturbed normal) ===
    // CR: fresnel = clamp(1.0 + dot(normalM, nViewPos), 0, 1)
    // viewDir is fragment-to-camera, so -viewDir = camera-to-fragment
    float fresnel = saturate(1.0 + dot(result.normalM, -viewDir));

    // CR Reimagined: fresnelM = pow3(fresnel) * 0.85 + 0.15
    // Guarantees minimum 15% reflection even at normal incidence (looking straight down)
    // This is stored in colortex4.a and used directly as blend factor in composite1
    result.fresnel = fresnel * fresnel * fresnel * 0.85 + 0.15;

    // Blend alpha toward opaque at grazing angles (CR: mix(alpha, 1.0, fresnel4))
    float fresnel4 = Pow2(Pow2(fresnel));
    result.alpha   = lerp(result.alpha, 1.0, fresnel4);

    // === Step 8: Smoothness ===
    // Reimagined style: fixed 0.5 smoothness
    result.smoothness = 0.5;

    return result;
}

#endif // LIB_WATER_HLSL
