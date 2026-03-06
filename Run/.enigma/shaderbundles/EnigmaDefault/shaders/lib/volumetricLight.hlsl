/**
 * @file volumetricLight.hlsl
 * @brief Screen-space volumetric light (god rays) via shadow map ray marching
 *
 * Ray marches from camera to fragment in world space, sampling the shadow map
 * at each step to accumulate lit segments. Directional modulation via VdotL
 * creates visible light shafts toward the sun/moon.
 *
 * Dependencies:
 *   - settings.hlsl    (LIGHTSHAFT_QUALI, VL_STRENGTH, SHADOW_DISTANCE)
 *   - common.hlsl      (SquaredLength)
 *   - shadow.hlsl      (WorldToShadowUV, SampleShadowMap)
 *   - celestial_uniforms (sunAngle)
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

// Night intensity reduction factor
// Ref: CR volumetricLight.glsl:50-54 — vlMultNightModifier ~0.3-0.5
// Adjusted for VL_STRENGTH=0.5 (net: 0.15*0.5=0.075, similar to previous 0.08*1.0)
static const float VL_NIGHT_MULT = 0.15;

//============================================================================//
// VL Color Palette (CR lightAndAmbientColors.glsl, COMPOSITE1 path)
// These are light shaft specific colors, separate from ground lighting.
//============================================================================//

// Noon: cool blue-white (sun overhead, scattered light)
// Ref: CR lightAndAmbientColors.glsl:8
static const float3 VL_NOON_COLOR = float3(0.4, 0.75, 1.3);

// Night: cold blue, very dim (reduced to match CR dark night lighting)
// Ref: CR lightAndAmbientColors.glsl:24
static const float3 VL_NIGHT_COLOR = float3(0.05, 0.08, 0.16);

//============================================================================//
// Main Entry Point
//============================================================================//

// VL-specific shadow sampling: sharp binary comparison without distance fade.
// Matches CR's texelFetch + 65536 approach (volumetricLight.glsl:176-177).
// Regular SampleShadowMap has distance fade that softens the result,
// preventing clean 0/1 values needed for the dual shadow test.
// Returns: 1.0 = lit, 0.0 = shadowed
float SampleShadowForVL(float3 shadowUV, Texture2D shadowTex, SamplerState samp)
{
    float shadowDepth = shadowTex.Sample(samp, shadowUV.xy).r;
    return saturate((shadowDepth - shadowUV.z) * 65536.0);
}

// Compute screen-space volumetric light via shadow map ray marching.
// Ray marches from camera toward the fragment in absolute world space,
// sampling the shadow map at each step to accumulate lit segments.
// Directional modulation matches CR's approach: linear VdotL + VdotU + vlTime.
//
// @param worldPos       Fragment position in ABSOLUTE world space
// @param cameraPos      Camera position in ABSOLUTE world space
// @param VdotL          dot(viewDirection, lightDirection) — directional falloff
// @param VdotU          dot(viewDirection, upDirection) — vertical falloff
// @param SdotU          dot(sunDirection, upDirection) — sun elevation
// @param dither         Screen-space dither value [0,1) to offset ray start
// @param vlFactor       Cloud depth modulation from colortex5.a (1.0 = no cloud)
// @param sunVisibility  Sun visibility [0,1] (computed from SdotU in caller)
// @param shadowView     Shadow view matrix (expects absolute world positions)
// @param shadowProj     Shadow projection matrix
// @param shadowTex0     Shadow depth texture including translucents (shadowtex0)
// @param shadowTex1     Shadow depth texture opaque-only (shadowtex1)
// @param shadowColTex   Shadow color texture for translucent light shafts (shadowcolor1)
// @param samp           Shadow sampler state
// @param eyeInWater     Eye-in-fluid state (EYE_IN_AIR/EYE_IN_WATER/etc.)
// @return               float3 volumetric color (additive, ready for sceneColor +=)
float3 GetVolumetricLight(
    float3       worldPos,
    float3       cameraPos,
    float        VdotL,
    float        VdotU,
    float        SdotU,
    float        dither,
    float        vlFactor,
    float        sunVisibility,
    float4x4     shadowView,
    float4x4     shadowProj,
    Texture2D    shadowTex0,
    Texture2D    shadowTex1,
    Texture2D    shadowColTex,
    SamplerState samp,
    int          eyeInWater)
{
    // --- vlTime: sun elevation gate ---
    // Fades VL to zero when sun is near horizon to avoid shadow map grazing-angle artifacts.
    // Ref: CR composite1.glsl:39 (original: deadzone=0.05, fade=0.15)
    // Widened to account for shadow map quality degradation at low sun angles.
    float vlTime = saturate((abs(SdotU) - VL_SUN_DEADZONE) / VL_SUN_FADE_RANGE);
    if (vlTime <= 0.0)
        return float3(0.0, 0.0, 0.0);

    // Determine day/night and select base sample count
    bool isDay       = sunVisibility >= 0.5;
    int  baseSamples = isDay ? VL_DAY_SAMPLES : VL_NIGHT_SAMPLES;

    // CR line 40: underwater forces vlSceneIntensity=1.0
    // This bypasses noon reduction and uses uniform sample weighting underwater,
    // ensuring colored light shafts through water remain visible at all sun angles.
    // Above water: night=0.0, day=vlFactor (cloud modulation)
    float vlSceneIntensity = (eyeInWater == EYE_IN_WATER)
                                 ? 1.0
                                 : (sunVisibility < 0.5)
                                 ? 0.0
                                 : vlFactor;
    int sampleCount = vlSceneIntensity < 0.5 ? baseSamples / 2 : baseSamples;
    sampleCount     = max(sampleCount, 4);

    // --- Directional modulation (CR approach) ---
    // Ref: CR volumetricLight.glsl:68-73
    // VdotLM: linear [0,1], NOT power — much wider visibility than pow(x, 8)
    float VdotLM = max((VdotL + 1.0) * 0.5, 0.0);

    // VdotUM: reduce VL when looking up (sky already bright)
    float VdotUmax0 = max(VdotU, 0.0);
    float VdotUM    = lerp(Pow2(1.0 - VdotUmax0), 1.0, 0.5 * vlSceneIntensity);
    VdotUM          = smoothstep(0.0, 1.0, VdotUM);

    // Combined directional multiplier
    float vlMult = VdotUM * VdotLM * vlTime;
    // CR has no VL_STRENGTH — underwater needs full intensity for visible light shafts.
    // Above water we apply VL_STRENGTH for tuning (CR relies on SALS instead).
    if (eyeInWater != EYE_IN_WATER)
        vlMult *= VL_STRENGTH;

    // --- Noon intensity reduction (CR volumetricLight.glsl:74) ---
    // At noon noonFactor~1 -> invNoonFactor2~0 -> base multiplier~0.125 (12.5%)
    // At sunrise/sunset noonFactor~0 -> invNoonFactor2~1 -> base multiplier~1.0 (100%)
    // CR uses mix(reduction, 1.0, vlSceneIntensity) but CR's vlFactor comes from SALS
    // (scene-aware light shafts, starts at 0). Our vlFactor defaults to 1.0 (no cloud),
    // so above-water mix would bypass reduction entirely. Only bypass underwater.
    float noonFactor     = sqrt(max(sin(sunAngle * TWO_PI), 0.0));
    float invNoonFactor  = 1.0 - noonFactor;
    float invNoonFactor2 = invNoonFactor * invNoonFactor;
    vlMult               *= (eyeInWater == EYE_IN_WATER) ? 1.0 : (invNoonFactor2 * 0.875 + 0.125);

    if (vlMult <= 0.001)
        return float3(0.0, 0.0, 0.0);

    // --- VL Color: compute BEFORE loop (needed for vlColorReducer) ---
    // Ref: CR lightAndAmbientColors.glsl:6-59, volumetricLight.glsl:38-59
    float  noonFactorDM   = noonFactor * noonFactor;
    float3 sunsetVLColor  = pow(float3(0.62, 0.39, 0.24), float3(1.5 + invNoonFactor, 1.5 + invNoonFactor, 1.5 + invNoonFactor)) * VL_SUNSET_COLOR_MULT;
    float3 dayVLColor     = lerp(sunsetVLColor, VL_NOON_COLOR, noonFactorDM);
    float  sunVisibility2 = sunVisibility * sunVisibility;
    float3 vlColor        = lerp(VL_NIGHT_COLOR, dayVLColor, sunVisibility2);

    // CR line 59: vlColorReducer prevents double-coloring of colored light shaft samples.
    // Colored samples from shadowcolor1 already have color; without reducer they get
    // tinted again by vlColor at the end. Reducer = 1/sqrt(vlColor) normalizes this.
    float3 vlColorReducer = isDay ? (1.0 / sqrt(max(vlColor, 0.001))) : float3(1.0, 1.0, 1.0);

    // --- Ray marching setup ---
    float3 rayVec    = worldPos - cameraPos;
    float  rayLength = length(rayVec);
    float  maxDist   = min(rayLength, SHADOW_DISTANCE * 0.9);
    // CR line 106: underwater uses shorter ray march distance (80.0)
    // Water attenuates light quickly, no need to march far
    if (eyeInWater == EYE_IN_WATER)
        maxDist = min(maxDist, 80.0);
    float3 rayDir = rayVec / max(rayLength, 0.001);

    float3 rayStep    = (rayDir * maxDist) / float(sampleCount);
    float3 currentPos = cameraPos + rayStep * dither;

    float shadowProjZ = shadowProj._m22;

    // --- Accumulate lit samples with per-sample weighting ---
    // Ref: CR volumetricLight.glsl:168-171
    // Further samples contribute more (percentComplete * 3.0 when no scene intensity)
    float4 volumetricLight = float4(0.0, 0.0, 0.0, 0.0);

    for (int i = 0; i < sampleCount; i++)
    {
        float3 shadowUV = WorldToShadowUV(currentPos, shadowView, shadowProj);

        // CR-style dual shadow test (volumetricLight.glsl:176-192):
        // 1. shadowtex0 (includes water) — primary occlusion
        // 2. shadowtex1 (opaque-only) — translucent shadow detection
        // 3. shadowcolor1 — colored light shafts through water
        float3 vlSample = float3(0.0, 0.0, 0.0);
        if (IsValidShadowUV(shadowUV))
        {
            float sampleDistSq = SquaredLength(currentPos - cameraPos);
            if (sampleDistSq < SHADOW_MAX_DIST_SQUARED)
            {
                // Above water: soft comparison (SampleShadowMap) for smooth VL without stripes
                // Underwater: sharp binary (SampleShadowForVL) for clean dual shadow test
                float shadow0 = (eyeInWater == EYE_IN_WATER)
                                    ? SampleShadowForVL(shadowUV, shadowTex0, samp)
                                    : SampleShadowMap(shadowUV, currentPos, shadowProjZ, shadowTex0, samp);
                vlSample = float3(shadow0, shadow0, shadow0);

                // Colored light shaft path: underwater only.
                // Above water uses standard white VL (shadow0 value).
                // CR runs this for both, but relies on translucentMult tinting
                // (which we don't have) to avoid artifacts at water boundaries.
                if (eyeInWater == EYE_IN_WATER)
                {
                    if (shadow0 < 0.5)
                    {
                        float shadow1 = SampleShadowForVL(shadowUV, shadowTex1, samp);
                        if (shadow1 > 0.5)
                        {
                            // Colored light shaft through water (CR: colsample * 4.0, squared)
                            float3 colSample = shadowColTex.Sample(samp, shadowUV.xy).rgb * 4.0;
                            colSample        *= colSample;
                            // CR line 190: normalize colored samples to prevent double-coloring
                            colSample *= vlColorReducer;
                            vlSample  = colSample;
                        }
                    }
                    else
                    {
                        // CR line 208: underwater, zero out fully-lit (non-translucent) samples.
                        // Underwater VL should only come from colored light shafts through
                        // the water surface, not from plain white VL.
                        vlSample = float3(0.0, 0.0, 0.0);
                    }
                }
            }
        }

        // Per-sample weight: ramp from 0 to 3 over ray length, blend with uniform when scene-aware
        // CR line 116: underwater uses 0.85 uniform weighting (slightly dimmer than above-water 1.0)
        float percentComplete   = float(i + 1) / float(sampleCount);
        float sampleMultIntense = (eyeInWater == EYE_IN_WATER) ? 0.85 : 1.0;
        float sampleMult        = lerp(percentComplete * 3.0, sampleMultIntense, vlSceneIntensity);
        sampleMult              /= float(sampleCount);

        // Near-camera fade: avoid VL popping at very close range
        float currentDist = length(currentPos - cameraPos);
        if (currentDist < 5.0)
            sampleMult *= smoothstep(0.0, 1.0, saturate(currentDist / 5.0));

        volumetricLight += float4(vlSample, 0.0) * sampleMult;
        currentPos      += rayStep;
    }

    // --- VL Color post-processing (CR volumetricLight.glsl:294-295) ---
    // vlColor base was computed before the loop; apply saturation/brightness here.
    // Saturation boost at sunset, reduce at noon
    float satPower = 0.5 + 0.5 * invNoonFactor;
    vlColor        = pow(max(vlColor, 0.001), float3(satPower, satPower, satPower));

    // Brightness modulation: boost at sunset, reduce at noon
    vlColor *= 1.0 + sunVisibility * Pow2(invNoonFactor);

    // Night intensity reduction (CR volumetricLight.glsl:50-54)
    if (!isDay)
    {
        vlMult *= VL_NIGHT_MULT;
    }

    // vlFactor intensity modulation: reduce brightness behind clouds
    vlMult *= vlFactor;

    return volumetricLight.rgb * vlColor * vlMult;
}

#endif // LIB_VOLUMETRIC_LIGHT_HLSL
