/// CR-style day/night lighting system for EnigmaDefault ShaderBundle
/// Reference: ComplementaryReimagined lib/lighting/mainLighting.glsl
/// Reference: ComplementaryReimagined lib/colors/lightAndAmbientColors.glsl

#ifndef LIB_LIGHTING_HLSL
#define LIB_LIGHTING_HLSL

#include "../include/settings.hlsl"
#include "common.hlsl"
#include "shadow.hlsl"

//============================================================================//
// Time Factor Functions
//============================================================================//

/// Sun visibility: 0 = night, 1 = day
/// CR: clamp(SdotU + 0.0625, 0, 0.125) / 0.125
/// Uses sunPosition and upPosition from celestial_uniforms (view space)
float GetSunVisibility(float3 inSunPosition, float3 inUpPosition)
{
    float SdotU = dot(normalize(inSunPosition), normalize(inUpPosition));
    return clamp(SdotU + 0.0625, 0.0, 0.125) / 0.125;
}

/// Shadow time: pow4 curve — 1.0 at noon/midnight, 0.0 at sunrise/sunset
/// Shadows fade to zero during transitions so only ambient light remains
float GetShadowTime(float sunVis)
{
    // Symmetric: shadows strongest when sun fully up or fully down
    // Weakest during transition (sunrise/sunset)
    float t = abs(sunVis * 2.0 - 1.0); // 0 at transition, 1 at noon/midnight
    return Pow4(t);
}

/// Noon factor: how close to solar noon. 1 = noon, 0 = night/horizon
/// CR: sqrt(max(sin(sunAngle * TWO_PI), 0))
float GetNoonFactor(float inSunAngle)
{
    return sqrt(max(sin(inSunAngle * TWO_PI), 0.0));
}

//============================================================================//
// Light & Ambient Color Functions
//============================================================================//

/// Day light color: lerp from sunset warm to noon neutral based on noonFactor
float3 GetDayLightColor(float noonFactor)
{
    return lerp(SUNSET_LIGHT_COLOR, NOON_LIGHT_COLOR, noonFactor);
}

/// Day ambient color: derived from skyColor with sunset tint blended in
float3 GetDayAmbientColor(float3 inSkyColor, float noonFactor)
{
    // Sky color provides base ambient; at sunset, warm tint bleeds in
    float3 skyAmbient    = inSkyColor * 1.4;
    float3 sunsetAmbient = lerp(inSkyColor, SUNSET_LIGHT_COLOR * 0.5, 0.3);
    return lerp(sunsetAmbient, skyAmbient, noonFactor);
}

/// Night light color: moonlight, scaled by player brightness setting
/// CR: 0.9 * vec3(0.15, 0.14, 0.20) * (0.4 + vsBrightness * 0.4)
float3 GetNightLightColor(float vsBrightness)
{
    return NIGHT_LIGHT_COLOR * (0.4 + vsBrightness * 0.4);
}

/// Night ambient color: dark blue sky glow, scaled by player brightness
/// CR: 0.9 * vec3(0.09, 0.12, 0.17) * (1.55 + vsBrightness * 0.77)
float3 GetNightAmbientColor(float vsBrightness)
{
    return NIGHT_AMBIENT_COLOR * (1.55 + vsBrightness * 0.77);
}

/// Combined scene light and ambient colors with day/night blending and rain
/// CR: lightAndAmbientColors.glsl main logic
void GetSceneLightColors(
    float      sunVis2, // sunVisibility squared for sharper transition
    float      noonFactor,
    float3     inSkyColor,
    float      vsBrightness,
    float      inRainStrength,
    out float3 outLightColor,
    out float3 outAmbientColor)
{
    // Day colors
    float3 dayLight   = GetDayLightColor(noonFactor);
    float3 dayAmbient = GetDayAmbientColor(inSkyColor, noonFactor);

    // Night colors
    float3 nightLight   = GetNightLightColor(vsBrightness);
    float3 nightAmbient = GetNightAmbientColor(vsBrightness);

    // Blend day ↔ night via sunVisibility²
    outLightColor   = lerp(nightLight, dayLight, sunVis2);
    outAmbientColor = lerp(nightAmbient, dayAmbient, sunVis2);

    // Rain darkening: reduce light, slightly boost ambient (overcast sky)
    // CR: lightColor *= 1.0 - rainStrength * 0.5
    outLightColor   *= 1.0 - inRainStrength * 0.5;
    outAmbientColor *= 1.0 + inRainStrength * 0.15;
}

//============================================================================//
// Block & Minimum Lighting
//============================================================================//

/// Block light with warm color and CR-style curve
/// CR: blocklightCol * pow(blockLight, 1.5) * blockLight
float3 GetBlockLighting(float blockLight)
{
    // Pow curve makes block light fall off more naturally
    float intensity = blockLight * blockLight * sqrt(blockLight); // ~pow(bl, 2.5)
    return BLOCKLIGHT_COLOR * intensity;
}

/// Cave minimum light — fades with skyLight, includes nightVision boost
/// Provides a tiny amount of light so caves aren't pitch black
float3 GetMinimumLighting(float skyLight, float vsBrightness, float inNightVision)
{
    // Base minimum: very dim, fades out as skyLight increases (outdoor = no need)
    float minBase = 0.003 * (1.0 + vsBrightness * 2.0);
    float skyFade = 1.0 - skyLight * skyLight; // caves (low skyLight) get more

    // Night vision provides significant boost
    float nvBoost = inNightVision * 0.25;

    return MIN_LIGHT_COLOR * (minBase * skyFade + nvBoost);
}

//============================================================================//
// Diffuse Calculation (unchanged)
//============================================================================//

/// Calculate diffuse lighting factor
float GetDiffuse(float skyLight, float3 normal, float3 lightDir, float fogMix, float rainStrength, bool isUnderwater)
{
    float NdotL         = dot(normalize(normal), normalize(lightDir));
    float baseDiffuse   = clamp(2.5 * NdotL, -0.3333, 1.0);
    float skyFactor     = Rescale(skyLight, 0.3137, 0.6235);
    float underwaterMod = isUnderwater ? 0.5 : 1.0;
    float fogMod        = 1.0 - fogMix;
    float rainMod       = 1.0 - rainStrength;
    return underwaterMod * fogMod * rainMod * skyFactor * baseDiffuse;
}

/// Simplified diffuse for thin objects (leaves, grass, etc.)
float GetDiffuseThin(float skyLight, float fogMix, float rainStrength, bool isUnderwater)
{
    float skyFactor     = Rescale(skyLight, 0.3137, 0.6235);
    float underwaterMod = isUnderwater ? 0.5 : 1.0;
    float fogMod        = 1.0 - fogMix;
    float rainMod       = 1.0 - rainStrength;
    return underwaterMod * fogMod * rainMod * skyFactor * 0.75;
}

//============================================================================//
// Light Strength (Diffuse + Shadow) (unchanged)
//============================================================================//

/// Calculate light strength combining diffuse and shadow
float GetLightStrength(float3    worldPos, float3        normal, float3 lightDir, float diffuse,
                       float4x4  shadowView, float4x4    shadowProj,
                       Texture2D shadowTex, SamplerState samp,
                       float2    screenPos)
{
    if (diffuse <= 0.0)
        return 0.0;

    float shadow = SampleShadowWithBias(worldPos, normal, lightDir,
                                        shadowView, shadowProj, shadowTex, samp,
                                        screenPos);
    return diffuse * shadow;
}

//============================================================================//
// Combined Lighting Pipeline (CR-style rewrite)
//============================================================================//

/// Full lighting calculation — CR mainLighting.glsl approach
/// Signature unchanged from previous version for deferred1.ps.hlsl compatibility
float3 CalculateLighting(
    float3       albedo,
    float3       worldPos,
    float3       normal,
    float3       lightDir,
    float        skyLight,
    float        blockLight,
    float        skyBrightness,
    float        sunAngle,
    float        fogMix,
    float        rainStrength,
    bool         isUnderwater,
    float        ao,
    float4x4     shadowView,
    float4x4     shadowProj,
    Texture2D    shadowTex,
    SamplerState samp,
    float2       screenPos)
{
    // --- Time factors ---
    float sunVis   = GetSunVisibility(sunPosition, upPosition);
    float sunVis2  = sunVis * sunVis; // sharper day/night transition
    float noonFac  = GetNoonFactor(sunAngle);
    float shadowTm = GetShadowTime(sunVis);

    // --- Sun elevation shadow gate ---
    // At very low sun angles, shadow map depth precision degrades and PCF
    // samples spread into invalid areas causing light leaks.
    // sunVisibility saturates to 1.0 at SdotU=0.0625 (too narrow), so shadowTime
    // doesn't help. This gate uses raw SdotU with a wider window.
    // Fade: 0 when SdotU < 0.05, ramp to 1.0 over [0.05, 0.20]
    float SdotU            = dot(normalize(sunPosition), normalize(upPosition));
    float sunElevationFade = saturate((SdotU - 0.05) / 0.15);

    // --- Player brightness (CR: vsBrightness) ---
    float vsBrightness = saturate(screenBrightness);

    // --- Scene light & ambient colors (CR day/night blend) ---
    float3 lightColor, ambientColor;
    GetSceneLightColors(sunVis2, noonFac, skyColor, vsBrightness, rainStrength,
                        lightColor, ambientColor);

    // --- Diffuse N·L factor ---
    float NdotLraw = dot(normalize(normal), normalize(lightDir));
    float NdotLM   = max(NdotLraw * 0.7 + 0.3, 0.0); // half-Lambert-ish

    // --- Raw shadow sampling ---
    float diffuse   = GetDiffuse(skyLight, normal, lightDir, fogMix, rainStrength, isUnderwater);
    float rawShadow = 0.0;
    if (diffuse > 0.0)
    {
        rawShadow = SampleShadowWithBias(worldPos, normal, lightDir,
                                         shadowView, shadowProj, shadowTex, samp,
                                         screenPos);
    }

    // --- Shadow multiplier (CR mainLighting.glsl) ---
    // shadowTime: shadows fade at sunrise/sunset (only ambient remains)
    // skyLightShadowMult: prevents shadow light leaking into caves
    float skyLightShadowMult = Pow4(skyLight) * Pow4(skyLight); // skyLight^8
    float shadowMult         = rawShadow * NdotLM * shadowTm * skyLightShadowMult * sunElevationFade;

    // --- Ambient multiplier: sky light drives ambient intensity ---
    float ambientMult = smoothstep(0.0, 1.0, skyLight);

    // --- Block lighting (warm torch color) ---
    float3 blockLighting = GetBlockLighting(blockLight);

    // --- Minimum lighting (cave floor + nightVision) ---
    float3 minLighting = GetMinimumLighting(skyLight, vsBrightness, nightVision);

    // --- Final composition (CR formula) ---
    // sceneLighting = lightColor * shadowMult + ambientColor * ambientMult
    // finalDiffuse  = sqrt(ao² * (blockLighting + sceneLighting² + minLighting))
    float3 sceneLighting = lightColor * shadowMult + ambientColor * ambientMult;
    float3 totalLight    = blockLighting + sceneLighting * sceneLighting + minLighting;
    float3 finalDiffuse  = sqrt(ao * ao * totalLight);

    return albedo * finalDiffuse;
}

#endif // LIB_LIGHTING_HLSL
