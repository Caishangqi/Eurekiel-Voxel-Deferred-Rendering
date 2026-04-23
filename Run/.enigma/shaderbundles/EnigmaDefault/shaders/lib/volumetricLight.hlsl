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
// @param translucentMult Per-pixel translucent color filter from colortex3 (1.0 = no translucent surface)
// @param depth0          Linear distance to translucent surface (depthtex0), used for water surface detection
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
    int          eyeInWater,
    float3       translucentMult,
    float        depth0)
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
    float3 rayDir    = rayVec / max(rayLength, 0.001);

    // CR line 106: maxDist = mix(far*0.55, 80.0, vlSceneIntensity)
    // CR uses far (render distance ~160), giving ~80-88 blocks.
    // We use SHADOW_DISTANCE as proxy. Key: stay within shadow map's
    // high-quality distortion region, NOT the full shadow distance.
    float maxDist = lerp(SHADOW_DISTANCE * 0.55, 80.0, vlSceneIntensity);
    if (eyeInWater == EYE_IN_WATER)
        maxDist = min(maxDist, 80.0);
    maxDist = min(maxDist, rayLength);

    float3 rayStep    = (rayDir * maxDist) / float(sampleCount);
    float3 currentPos = cameraPos + rayStep * dither;

    // --- Accumulate lit samples ---
    // CR volumetricLight.glsl:141-231
    float4 volumetricLight = float4(0.0, 0.0, 0.0, 0.0);

    for (int i = 0; i < sampleCount; i++)
    {
        float currentDist = length(currentPos - cameraPos);

        // CR line 156-157: default to lit — outside shadow map = no occlusion info
        float  shadowSample = 1.0;
        float3 vlSample     = float3(1.0, 1.0, 1.0);

        float3 shadowUV = WorldToShadowUV(currentPos, shadowView, shadowProj);

        // CR line 174: circular boundary = shadow map distortion coverage area.
        // Inside circle: query shadow map for actual occlusion.
        // Outside circle: keep default lit (no shadow data available).
        float2 shadowClip = shadowUV.xy * 2.0 - 1.0;
        if (dot(shadowClip, shadowClip) < 1.0)
        {
            // CR line 176-177: sharp binary comparison (texelFetch + 65536)
            shadowSample = SampleShadowForVL(shadowUV, shadowTex0, samp);
            vlSample     = float3(shadowSample, shadowSample, shadowSample);

            // Colored light shaft path (CR line 181-210)
            if (shadowSample < 0.5)
            {
                float shadow1 = SampleShadowForVL(shadowUV, shadowTex1, samp);
                if (shadow1 > 0.5)
                {
                    float3 colSample = shadowColTex.Sample(samp, shadowUV.xy).rgb * 4.0;
                    colSample        *= colSample;
                    colSample        *= vlColorReducer;
                    vlSample         = colSample;
                    shadowSample     = 1.0;
                }
            }
            else
            {
                // CR line 208: underwater, fully lit but no translucent surface = no VL
                if (eyeInWater == EYE_IN_WATER && all(translucentMult >= 0.999))
                    vlSample = float3(0.0, 0.0, 0.0);
            }
        }

        // CR line 214: translucent tinting OUTSIDE circle check
        if (currentDist > depth0)
            vlSample *= translucentMult;

        // Per-sample weight (CR line 168-171)
        float percentComplete   = currentDist / max(maxDist, 0.001);
        float sampleMultIntense = (eyeInWater == EYE_IN_WATER) ? 0.85 : 1.0;
        float sampleMult        = lerp(percentComplete * 3.0, sampleMultIntense, vlSceneIntensity);
        if (currentDist < 5.0)
            sampleMult *= smoothstep(0.0, 1.0, saturate(currentDist / 5.0));
        sampleMult /= float(sampleCount);

        volumetricLight += float4(vlSample, shadowSample) * sampleMult;
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
