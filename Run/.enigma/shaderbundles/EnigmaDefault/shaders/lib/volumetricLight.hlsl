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
// @param shadowTex      Shadow depth texture (shadowtex1)
// @param samp           Shadow sampler state
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
    Texture2D    shadowTex,
    SamplerState samp)
{
    // --- vlTime: sun elevation gate ---
    // Ref: CR composite1.glsl:39 — vlTime = min(abs(SdotU) - 0.05, 0.15) / 0.15
    // Fades VL to zero when sun is near horizon (|SdotU| < 0.05)
    float vlTime = saturate((abs(SdotU) - 0.05) / 0.15);
    if (vlTime <= 0.0)
        return float3(0.0, 0.0, 0.0);

    // Determine day/night and select base sample count
    bool isDay       = sunVisibility >= 0.5;
    int  baseSamples = isDay ? VL_DAY_SAMPLES : VL_NIGHT_SAMPLES;

    // vlFactor modulation: clouds reduce sample count for performance
    float vlSceneIntensity = vlFactor;
    int   sampleCount      = vlSceneIntensity < 0.5 ? baseSamples / 2 : baseSamples;
    sampleCount            = max(sampleCount, 4);

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
    vlMult       *= VL_STRENGTH;

    // --- Noon intensity reduction (CR volumetricLight.glsl:74) ---
    // At noon noonFactor≈1 → invNoonFactor2≈0 → multiplier≈0.125 (12.5%)
    // At sunrise/sunset noonFactor≈0 → invNoonFactor2≈1 → multiplier≈1.0 (100%)
    // This prevents VL from being blindingly bright when the sun is overhead.
    //
    // NOTE: CR uses mix(reduction, 1.0, vlSceneIntensity) to bypass reduction when
    // clouds are present, but CR's vlFactor is a global flat varying (corner-pixel
    // temporal accumulation), NOT per-pixel cloudLinearDepth like ours.
    // Our cloudLinearDepth defaults to 1.0 (no cloud) which would bypass the
    // reduction entirely. So we apply the reduction unconditionally.
    // Ref: CR common.glsl:681, CR volumetricLight.glsl:74
    float noonFactor     = sqrt(max(sin(sunAngle * TWO_PI), 0.0));
    float invNoonFactor  = 1.0 - noonFactor;
    float invNoonFactor2 = invNoonFactor * invNoonFactor;
    vlMult               *= invNoonFactor2 * 0.875 + 0.125;

    if (vlMult <= 0.001)
        return float3(0.0, 0.0, 0.0);

    // --- Ray marching setup ---
    float3 rayVec    = worldPos - cameraPos;
    float  rayLength = length(rayVec);
    float  maxDist   = min(rayLength, SHADOW_DISTANCE * 0.9);
    float3 rayDir    = rayVec / max(rayLength, 0.001);

    float3 rayStep    = (rayDir * maxDist) / float(sampleCount);
    float3 currentPos = cameraPos + rayStep * dither;

    float shadowProjZ = shadowProj._m22;

    // --- Accumulate lit samples with per-sample weighting ---
    // Ref: CR volumetricLight.glsl:168-171
    // Further samples contribute more (percentComplete * 3.0 when no scene intensity)
    float4 volumetricLight = float4(0.0, 0.0, 0.0, 0.0);

    for (int i = 0; i < sampleCount; i++)
    {
        float3 shadowUV    = WorldToShadowUV(currentPos, shadowView, shadowProj);
        float  shadowValue = SampleShadowMap(shadowUV, currentPos, shadowProjZ, shadowTex, samp);

        // Per-sample weight: ramp from 0 to 3 over ray length, blend with uniform when scene-aware
        float percentComplete = float(i + 1) / float(sampleCount);
        float sampleMult      = lerp(percentComplete * 3.0, 1.0, vlSceneIntensity);
        sampleMult            /= float(sampleCount);

        // Near-camera fade: avoid VL popping at very close range
        float currentDist = length(currentPos - cameraPos);
        if (currentDist < 5.0)
            sampleMult *= smoothstep(0.0, 1.0, saturate(currentDist / 5.0));

        volumetricLight += float4(shadowValue, shadowValue, shadowValue, shadowValue) * sampleMult;
        currentPos      += rayStep;
    }

    // --- VL Color: time-aware dynamic color (CR approach) ---
    // Ref: CR lightAndAmbientColors.glsl:6-59, volumetricLight.glsl:293-295
    //
    // CR uses noonFactor² for light shaft mixing (sharper transition than ground lighting)
    // Sunset color uses pow() with invNoonFactor to deepen warm tones at low sun angles
    float noonFactorDM = noonFactor * noonFactor; // light shaft factor (CR line 54)

    // Sunset: warm orange, pow() deepens color at low angles
    // Ref: CR lightAndAmbientColors.glsl:15
    // Multiplier reduced from 6.8 → 3.0 to prevent sunrise/sunset VL washout
    float3 sunsetVLColor = pow(float3(0.62, 0.39, 0.24), float3(1.5 + invNoonFactor, 1.5 + invNoonFactor, 1.5 + invNoonFactor)) * 3.0;

    // Day color: blend sunset → noon based on noonFactor²
    float3 dayVLColor = lerp(sunsetVLColor, VL_NOON_COLOR, noonFactorDM);

    // Final: blend night → day based on sunVisibility²
    float  sunVisibility2 = sunVisibility * sunVisibility;
    float3 vlColor        = lerp(VL_NIGHT_COLOR, dayVLColor, sunVisibility2);

    if (!isDay)
    {
        vlMult *= VL_NIGHT_MULT;
    }

    // --- VL Color post-processing (CR volumetricLight.glsl:294-295) ---
    // Saturation boost at sunset, reduce at noon
    // Ref: CR line 294 — pow(vlColor, 0.5 + 0.5*invNoonFactor + 0.3*rain)
    // We omit rainFactor (not available here), simplified to sunset saturation boost
    float satPower = 0.5 + 0.5 * invNoonFactor;
    vlColor        = pow(max(vlColor, 0.001), float3(satPower, satPower, satPower));

    // Brightness modulation: boost at sunset, reduce at noon
    // Ref: CR line 295 — vlColor *= 1.0 - ... + sunVisibility * pow2(invNoonFactor) * invRainFactor
    vlColor *= 1.0 + sunVisibility * Pow2(invNoonFactor);

    // vlFactor intensity modulation: reduce brightness behind clouds
    vlMult *= vlFactor;

    return volumetricLight.rgb * vlColor * vlMult;
}

#endif // LIB_VOLUMETRIC_LIGHT_HLSL
