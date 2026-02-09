/// [NEW] Lighting calculation utilities for EnigmaDefault ShaderBundle
/// Reference: miniature-shader/shaders/common/getDiffuse.vsh
/// Reference: miniature-shader/shaders/common/getLightStrength.fsh

#ifndef LIB_LIGHTING_HLSL
#define LIB_LIGHTING_HLSL

#include "../include/settings.hlsl"
#include "common.hlsl"
#include "shadow.hlsl"

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
/// @param worldPos World space position
/// @param diffuse Pre-calculated diffuse factor
/// @param shadowView Shadow view matrix
/// @param shadowProj Shadow projection matrix
/// @param shadowTex Shadow depth texture
/// @param samp Sampler state
/// @return Light strength [0,1]
float GetLightStrength(float3 worldPos, float diffuse, float4x4 shadowView, float4x4 shadowProj, Texture2D shadowTex, SamplerState samp)
{
    // Early out if no diffuse contribution
    if (diffuse <= 0.0)
        return diffuse;

    // Sample shadow
    float3 shadowUV = WorldToShadowUV(worldPos, shadowView, shadowProj);
    float  shadow   = SampleShadowMap(shadowUV, worldPos, shadowTex, samp);

    return diffuse * shadow;
}

/// Simplified version using engine shadowtex1
float GetLightStrength(float3 worldPos, float diffuse, float4x4 shadowView, float4x4 shadowProj, SamplerState samp)
{
    if (diffuse <= 0.0)
        return diffuse;

    float shadow = SampleShadow(worldPos, shadowView, shadowProj, samp);
    return diffuse * shadow;
}

//============================================================================//
// Final Lighting Application
//============================================================================//

/// Apply lighting to albedo color
/// @param albedo Base color
/// @param lightStrength Combined light strength [0,1]
/// @param lightColor Light color (usually sun/moon color)
/// @return Lit color
float3 ApplyLighting(float3 albedo, float lightStrength, float3 lightColor)
{
    // Blend between shadow color and full light based on strength
    float3 shadowedLight = lerp(SHADOW_COLOR, lightColor, lightStrength);

    // Apply brightness multiplier
    return albedo * shadowedLight * LIGHT_BRIGHTNESS;
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
float3 CalculateLighting(
    float3       albedo,
    float3       worldPos,
    float3       normal,
    float3       lightDir,
    float3       lightColor,
    float        skyLight,
    float        fogMix,
    float        rainStrength,
    bool         isUnderwater,
    float4x4     shadowView,
    float4x4     shadowProj,
    SamplerState samp)
{
    float diffuse       = GetDiffuse(skyLight, normal, lightDir, fogMix, rainStrength, isUnderwater);
    float lightStrength = GetLightStrength(worldPos, diffuse, shadowView, shadowProj, samp);
    return ApplyLighting(albedo, lightStrength, lightColor);
}

#endif // LIB_LIGHTING_HLSL
