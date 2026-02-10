/// [NEW] Lighting calculation utilities for EnigmaDefault ShaderBundle
/// Reference: miniature-shader/shaders/common/getDiffuse.vsh
/// Reference: miniature-shader/shaders/common/getLightStrength.fsh
/// Reference: miniature-shader/shaders/common/getLightColor.vsh

#ifndef LIB_LIGHTING_HLSL
#define LIB_LIGHTING_HLSL

#include "../include/settings.hlsl"
#include "common.hlsl"
#include "shadow.hlsl"

//============================================================================//
// Light Color (Day/Night Cycle)
//============================================================================//

/// Calculate sun height from sunAngle (Iris convention)
/// In Iris, sunPosition has magnitude ~100, so sunHeight ranges [-100, 100]
/// @param sunAngle Iris sunAngle (0.0-1.0 cycle)
///   IMPORTANT: In our engine this is `compensatedCelestialAngle`, NOT `celestialAngle`
///   compensatedCelestialAngle = celestialAngle + 0.25 = Iris sunAngle
///   0.25 = noon, 0.50 = sunset, 0.75 = midnight, 0.9675 = sunrise
/// @return Sun height value matching miniature-shader's view2feet(sunPosition).y
float GetSunHeight(float sunAngle)
{
    return 100.0 * sin(sunAngle * 6.28318530718);
}

/// Calculate light color based on sun height (day/night cycle)
/// Ref: miniature-shader common/getLightColor.vsh
/// @param sunHeight Sun height in world space (~[-100, 100])
/// @param skyLight  Sky light value [0,1] from lightmap
/// @return Light color for the current time of day
float3 GetLightColor(float sunHeight, float skyLight)
{
    // Sun redness at sunrise/sunset (low sun angles → more red)
    // Transition: sunHeight 19.6-24.6 → redness fades out
    float sunRedness = 1.0 - clamp(0.2 * sunHeight - 3.929, 0.0, 1.0);

    // Day: warm sun color with redness; Night: cool moon color
    float3 lightColor = sunHeight > 0.01
                            ? normalize(float3(1.0 + clamp(sunRedness, 0.12, 1.0), 1.06, 1.0))
                            : MOON_COLOR;

    // Fade transition between night and day
    // Full brightness when |sunHeight| > 14.5, zero when < 4.5
    lightColor *= clamp(0.1 * abs(sunHeight) - 0.4453, 0.0, 1.0);

    // Reduce color saturation on dark spots (low skyLight)
    float l = Luma(lightColor);
    return lerp(float3(l, l, l), lightColor, skyLight);
}

//============================================================================//
// Diffuse Calculation
//============================================================================//

/// Calculate diffuse lighting factor
/// @param skyLight Sky light value [0,1]
/// @param normal Surface normal (world space)
/// @param lightDir Light direction (normalized)
/// @param fogMix Fog blend factor [0,1]
/// @param rainStrength Rain intensity [0,1]
/// @param isUnderwater True if camera is underwater
/// @return Diffuse factor [0,1]
float GetDiffuse(float skyLight, float3 normal, float3 lightDir, float fogMix, float rainStrength, bool isUnderwater)
{
    // Base diffuse from normal dot light
    float NdotL       = dot(normalize(normal), normalize(lightDir));
    float baseDiffuse = clamp(2.5 * NdotL, -0.3333, 1.0);

    // Sky light contribution (rescale from lightmap range)
    float skyFactor = Rescale(skyLight, 0.3137, 0.6235);

    // Environmental modifiers
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
// Light Strength (Diffuse + Shadow)
//============================================================================//

/// Calculate light strength combining diffuse and shadow
/// Ref: miniature-shader getLightStrength.fsh — diffuse modulates shadow result
float GetLightStrength(float3    worldPos, float3        normal, float3 lightDir, float diffuse,
                       float4x4  shadowView, float4x4    shadowProj,
                       Texture2D shadowTex, SamplerState samp)
{
    // Early out if no diffuse contribution (back-face or no sky light)
    // Ref: miniature-shader getLightStrength.fsh:6 — "if diffuse > 0.0"
    if (diffuse <= 0.0)
        return 0.0;

    // Shadow sampling with normal bias + soft comparison + SHADOW_PIXEL
    float shadow = SampleShadowWithBias(worldPos, normal, lightDir,
                                        shadowView, shadowProj, shadowTex, samp);

    return diffuse * shadow;
}

//============================================================================//
// Final Lighting Application
//============================================================================//

/// Apply lighting to albedo color
/// Ref: miniature-shader textured_lit.fsh:115-116
/// Two-step process:
///   1. Shadow tint: lerp(SHADOW_COLOR, white, lightStrength)
///   2. Brightness boost with light color (normalized → safe from overexposure)
/// @param albedo       Base color
/// @param lightStrength Combined light strength [0,1]
/// @param lightColor   Light color from GetLightColor (normalized sun or MOON_COLOR)
/// @return Lit color
float3 ApplyLighting(float3 albedo, float lightStrength, float3 lightColor)
{
    // Step 1: Shadow color blend — dark areas get SHADOW_COLOR tint
    // Ref: textured_lit.fsh:115 — "ambient *= mix(SHADOW_COLOR, vec3(1.0), lightStrength)"
    float3 shadowedLight = lerp(SHADOW_COLOR, float3(1.0, 1.0, 1.0), lightStrength);

    // Step 2: Brightness boost with light color tint
    // Ref: textured_lit.fsh:116 — "ambient *= 0.75 + (lightBrightness * lightStrength) * lightColor"
    // lightBrightness reduces boost for bright surfaces to prevent overexposure
    float  albedoLuma      = Luma(albedo);
    float  lightBrightness = max(0.0, LIGHT_BRIGHTNESS - 0.5 * Pow3(albedoLuma));
    float3 brightnessBoost = 0.75 + (lightBrightness * lightStrength) * lightColor;

    return albedo * shadowedLight * brightnessBoost;
}

/// Apply lighting with default white light
float3 ApplyLighting(float3 albedo, float lightStrength)
{
    return ApplyLighting(albedo, lightStrength, float3(1.0, 1.0, 1.0));
}

//============================================================================//
// Combined Lighting Pipeline
//============================================================================//

/// Full lighting calculation: diffuse + shadow + apply
/// Ref: miniature-shader textured_lit.fsh full pipeline
/// @param sunAngle Iris sunAngle (= engine compensatedCelestialAngle), used to compute light color
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
    SamplerState samp)
{
    // Light color from day/night cycle
    // Ref: miniature-shader textured_lit.vsh — getLightColor(sunHeight, skyLight)
    float  sunHeight  = GetSunHeight(sunAngle);
    float3 lightColor = GetLightColor(sunHeight, skyLight);

    // Base ambient level (proxy for miniature-shader's getAmbientColor)
    // In miniature-shader, the lightmap texture encodes day/night brightness;
    // here skyBrightness serves the same role for daytime.
    // At night, skyBrightness drops near zero, but outdoor areas still receive
    // moonlight — we approximate this with a skyLight-based floor.
    float ambientLevel = max(blockLight, skyLight * skyBrightness);

    // Moonlight ambient floor: outdoor areas (high skyLight) stay visible at night
    // Indoor/cave areas (low skyLight) remain dark as intended
    // Ref: Minecraft lightmap provides ~0.08-0.15 ambient at night with full sky light
    ambientLevel = max(ambientLevel, skyLight * 0.15);

    // Absolute minimum (deep caves, no light sources)
    ambientLevel = max(ambientLevel, 0.03);

    // Diffuse factor (N·L, sky light, environment modifiers)
    float diffuse = GetDiffuse(skyLight, normal, lightDir, fogMix, rainStrength, isUnderwater);

    // Combined diffuse × shadow
    float lightStrength = GetLightStrength(worldPos, normal, lightDir, diffuse,
                                           shadowView, shadowProj, shadowTex, samp);

    // Block light provides a floor (ref: miniature-shader textured_lit.fsh:111)
    lightStrength = max(0.75 * blockLight, lightStrength);

    // Apply lighting with shadow color + brightness boost + AO
    // ambientLevel provides day/night base brightness
    return ApplyLighting(albedo, lightStrength, lightColor) * ambientLevel * ao;
}

#endif // LIB_LIGHTING_HLSL
