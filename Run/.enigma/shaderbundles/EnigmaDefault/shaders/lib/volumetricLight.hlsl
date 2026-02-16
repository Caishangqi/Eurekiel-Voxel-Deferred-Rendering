/**
 * @file volumetricLight.hlsl
 * @brief Screen-space volumetric light (god rays) via shadow map ray marching
 *
 * Ray marches from camera to fragment in world space, sampling the shadow map
 * at each step to accumulate lit segments. Directional modulation via VdotL
 * creates visible light shafts toward the sun/moon.
 *
 * Dependencies:
 *   - settings.hlsl    (LIGHTSHAFT_QUALI, VL_STRENGTH, SHADOW_DISTANCE, MOON_COLOR)
 *   - common.hlsl      (SquaredLength)
 *   - shadow.hlsl      (WorldToShadowUV, SampleShadowMap)
 *
 * Reference:
 *   - ComplementaryReimagined volumetricLight.glsl:1-330
 *
 * Requirements: R4.4.1, R4.4.2, R4.4.3
 */

#ifndef LIB_VOLUMETRIC_LIGHT_HLSL
#define LIB_VOLUMETRIC_LIGHT_HLSL

//============================================================================//
// Quality Tier Sample Counts
// Day samples are higher because sun shafts are more prominent
//============================================================================//

static const int VL_DAY_SAMPLES =
#if LIGHTSHAFT_QUALI == 1
    12;
#elif LIGHTSHAFT_QUALI == 2
    20;
#elif LIGHTSHAFT_QUALI == 3
30;
#else // LIGHTSHAFT_QUALI == 4
50;
#endif

static const int VL_NIGHT_SAMPLES =
#if LIGHTSHAFT_QUALI == 1
    6;
#elif LIGHTSHAFT_QUALI == 2
    10;
#elif LIGHTSHAFT_QUALI == 3
15;
#else // LIGHTSHAFT_QUALI == 4
30;
#endif

// VdotL directional falloff exponent
// Higher = tighter shafts concentrated toward light source
static const float VL_VDOTL_POWER = 8.0;

// Sun volumetric light color (warm tone)
static const float3 VL_SUN_COLOR = float3(1.0, 0.95, 0.85);

// Night intensity reduction factor
static const float VL_NIGHT_MULT = 0.3;

//============================================================================//
// Main Entry Point
//============================================================================//

// Compute screen-space volumetric light via shadow map ray marching.
// Ray marches from camera (origin) toward the fragment, accumulating
// light contribution where the shadow map indicates no occlusion.
//
// @param worldPos       Fragment position relative to camera (world units)
// @param VdotL          dot(viewDirection, lightDirection) for directional falloff
// @param dither         Screen-space dither value [0,1) to offset ray start
// @param vlFactor       Cloud depth modulation from colortex5.a (1.0 = no cloud)
// @param sunVisibility  Sun visibility [0,1] (computed from SdotU in caller)
// @param shadowView     Shadow view matrix
// @param shadowProj     Shadow projection matrix
// @param shadowTex      Shadow depth texture (shadowtex1)
// @param samp           Shadow sampler state
// @return               float4(RGB volumetric color, accumulated intensity)
float4 GetVolumetricLight(
    float3       worldPos,
    float        VdotL,
    float        dither,
    float        vlFactor,
    float        sunVisibility,
    float4x4     shadowView,
    float4x4     shadowProj,
    Texture2D    shadowTex,
    SamplerState samp)
{
    // Determine day/night and select base sample count
    bool isDay       = sunVisibility >= 0.5;
    int  baseSamples = isDay ? VL_DAY_SAMPLES : VL_NIGHT_SAMPLES;

    // vlFactor modulation: clouds reduce sample count for performance
    // Ref: CR volumetricLight.glsl:21-33
    int sampleCount = vlFactor < 0.5 ? baseSamples / 2 : baseSamples;
    sampleCount     = max(sampleCount, 4);

    // Clamp ray length to shadow distance (no shadow data beyond)
    float  rayLength = length(worldPos);
    float  maxDist   = min(rayLength, SHADOW_DISTANCE * 0.9);
    float3 rayDir    = worldPos / max(rayLength, 0.001);
    float3 rayEnd    = rayDir * maxDist;

    // Ray step vector with dithered start to reduce banding
    float3 rayStep    = rayEnd / float(sampleCount);
    float3 currentPos = rayStep * dither;

    float shadowProjZ = shadowProj._m22;

    // Accumulate lit samples
    float accumLight = 0.0;

    for (int i = 0; i < sampleCount; i++)
    {
        // Reuse shadow infrastructure (DRY): WorldToShadowUV + SampleShadowMap
        float3 shadowUV    = WorldToShadowUV(currentPos, shadowView, shadowProj);
        float  shadowValue = SampleShadowMap(shadowUV, currentPos, shadowProjZ, shadowTex, samp);

        // shadowValue: 1.0 = lit (light passes through), 0.0 = shadowed
        accumLight += shadowValue;
        currentPos += rayStep;
    }

    accumLight /= float(sampleCount);

    // VdotL directional modulation: light shafts strongest toward light source
    // pow((VdotL + 1) / 2, N) maps [-1,1] to [0,1] with power falloff
    float vdotlFactor = pow(saturate((VdotL + 1.0) * 0.5), VL_VDOTL_POWER);
    accumLight        *= vdotlFactor;

    // vlFactor intensity modulation: reduce brightness behind clouds
    accumLight *= vlFactor;

    // Apply user-configurable strength
    accumLight *= VL_STRENGTH;

    // Time-of-day color and intensity
    float3 vlColor;
    if (isDay)
    {
        vlColor = VL_SUN_COLOR;
    }
    else
    {
        vlColor    = MOON_COLOR;
        accumLight *= VL_NIGHT_MULT;
    }

    return float4(vlColor * accumLight, accumLight);
}

#endif // LIB_VOLUMETRIC_LIGHT_HLSL
