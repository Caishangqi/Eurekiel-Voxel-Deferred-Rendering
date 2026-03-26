/// Sun/Moon sky glare library (CR sky.glsl port)
/// Fresnel-like atmospheric halo around sun/moon.
/// Appears as bright white/yellow glow when looking near the sun,
/// visible at block edges silhouetted against the sky.
///
/// Architecture:
///   gbuffers_skybasic calls GetSkyGlare() in SKY phase, adds to sky color.
///   Pure math: Fresnel-like formula from view-sun angle.
///
/// Dependencies:
///   - core.hlsl (sunPosition, upPosition, isEyeInWater, rainStrength)
///   - lib/common.hlsl (Pow4, Luma)
///
/// Reference: ComplementaryReimagined lib/atmospherics/sky.glsl lines 57-88

#ifndef LIB_SKY_GLARE_HLSL
#define LIB_SKY_GLARE_HLSL

/// Compute sun/moon glare contribution for a sky pixel.
///
/// @param nViewDir     Normalized view direction (world space)
/// @param sunDirWorld  Normalized sun direction (world space)
/// @param sunVis       Sun visibility [0,1] (0=night, 1=day)
/// @param shadowTime   Shadow time factor [0,1] (0=sunrise/sunset, 1=noon/midnight)
/// @param noonFactor   Noon factor [0,1] (1=noon, 0=night/horizon)
/// @return             Additive glare color (HDR)
float3 GetSkyGlare(float3 nViewDir, float3 sunDirWorld,
                   float sunVis, float shadowTime, float noonFactor)
{
    // VdotS: view dot sun direction
    // Positive = looking toward sun, negative = looking toward moon
    float VdotS = dot(nViewDir, sunDirWorld);

    // Select sun or moon based on time of day
    // Day: glare around sun (VdotS > 0)
    // Night: glare around moon (-VdotS > 0)
    float VdotSML = sunVis > 0.5 ? VdotS : -VdotS;

    if (VdotSML <= 0.0)
        return float3(0.0, 0.0, 0.0);

    // --- Fresnel-like glare formula (CR sky.glsl:60-67) ---
    // glareScatter: adaptive exponent (6.0 far from sun, ~3.0 near sun center)
    float glareScatter = 3.0 * (2.0 - saturate(VdotS * 1000.0));

    // Rain widens and dims the glare
    float rainFactor2 = rainStrength * rainStrength;
    glareScatter *= 1.0 - 0.75 * rainFactor2;

    float VdotSM4 = pow(abs(VdotS), glareScatter);

    // Fresnel-like inverse formula: bright core, smooth falloff
    float visfactor = 0.075;
    float glare = visfactor / (1.0 - (1.0 - visfactor) * VdotSM4) - visfactor;
    glare *= 0.7;

    // --- Glare color (CR sky.glsl:70-72) ---
    // Moon: cool blue-grey, Sun: warm yellow-orange shifting to cyan at noon
    float3 moonColor = float3(0.38, 0.4, 0.5) * 0.3;
    float3 sunColor  = float3(1.5, 0.7, 0.3) + float3(0.0, 0.5, 0.5) * noonFactor;
    float3 glareColor = lerp(moonColor, sunColor, sunVis);

    // Underwater: massive brightness boost (sun seen through water surface)
    float glareWaterFactor = (isEyeInWater != EYE_IN_AIR) ? sunVis : 0.0;
    glareColor += glareWaterFactor * 7.0;

    // --- Rain attenuation (CR sky.glsl:74-84) ---
    glare *= 1.0 - 0.8 * rainStrength;

    // Rain desaturation: glare becomes grey in overcast
    float glareDesatFactor = 0.5 * rainStrength;
    float glareLuma = Luma(glareColor);
    glareColor = lerp(glareColor, float3(glareLuma, glareLuma, glareLuma), glareDesatFactor);

    return glareColor * glare * shadowTime;
}

#endif // LIB_SKY_GLARE_HLSL
