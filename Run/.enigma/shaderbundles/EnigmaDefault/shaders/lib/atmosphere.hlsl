/**
 * @file atmosphere.hlsl
 * @brief Atmospheric distance fog for deferred pass
 *
 * Computes fog color from sky color, sun angle, and weather — NOT from the
 * fogColor uniform (that is reserved for fluid fog in @engine/lib/fog.hlsl).
 *
 * Dependencies:
 *   - core.hlsl        (TWO_PI)
 *   - common.hlsl      (Luma)
 *   - settings.hlsl    (OVERWORLD_FOG_MIN, OVERWORLD_FOG_MAX — optional future use)
 *   - common_uniforms  (skyColor, rainStrength)
 *   - celestial_uniforms (sunAngle)
 *   - fog_uniforms     (fogStart, fogEnd)
 *
 * Reference:
 *   - ComplementaryReimagined lib/colors/skyColors.glsl
 *   - miniature-shader getFogMix.vsh / getFogColor.vsh
 *
 * Requirements: R4.2.1, R4.2.2, R4.2.3
 */

#ifndef LIB_ATMOSPHERE_HLSL
#define LIB_ATMOSPHERE_HLSL

// Compute atmospheric fog color from sky color, sun angle, and weather.
// Fog color varies with time of day (sunset warmth, night desaturation)
// and weather (rain darkening).
//
// @param sunAngle     Iris-compatible sun angle (0-1 normalized cycle)
// @param inSkyColor   CPU-calculated sky color from CommonUniforms
// @return             Fog color for atmospheric blending
float3 ComputeAtmosphericFogColor(float sunAngle, float3 inSkyColor)
{
    // Base: sky color at horizon level
    float3 fogCol = inSkyColor;

    // Sunset/sunrise warm tint
    // sunAngle is already Iris sunAngle — no +0.25 offset needed
    float sunHeight    = sin(sunAngle * TWO_PI);
    float sunsetFactor = 1.0 - saturate(abs(sunHeight) * 5.0);
    fogCol             = lerp(fogCol, fogCol * float3(1.2, 0.9, 0.7), sunsetFactor * 0.3);

    // Night desaturation
    float nightFactor = saturate(-sunHeight * 3.0);
    float fogLuma     = Luma(fogCol);
    fogCol            = lerp(fogCol, float3(fogLuma, fogLuma, fogLuma) * 0.15, nightFactor);

    // Rain darkening
    fogCol = lerp(fogCol, fogCol * 0.4, rainStrength);

    return fogCol;
}

// Compute distance-based atmospheric fog factor with quadratic falloff.
// Sky pixels (depth >= 1.0) should be excluded by the caller before
// invoking this function.
//
// @param viewDistance  Linear distance from camera to fragment (world units)
// @param fogStart     Near fog boundary (from FogUniforms, ImGui-controlled)
// @param fogEnd       Far fog boundary (from FogUniforms, ImGui-controlled)
// @return             Fog blend factor in [0, 1], quadratic falloff
float GetAtmosphericFogFactor(float viewDistance, float fogStart, float fogEnd)
{
    float rawFog = saturate((viewDistance - fogStart) / max(fogEnd - fogStart, 0.001));
    return rawFog * rawFog; // Quadratic falloff for natural feel
}

// Blend scene color toward fog color by the given fog factor.
//
// @param sceneColor   Original lit scene color
// @param fogColor     Atmospheric fog color (from ComputeAtmosphericFogColor)
// @param fogFactor    Blend factor in [0, 1] (from GetAtmosphericFogFactor)
// @return             Fogged scene color
float3 ApplyAtmosphericFog(float3 sceneColor, float3 fogColor, float fogFactor)
{
    return lerp(sceneColor, fogColor, fogFactor);
}

// ============================================================================
// CR-style Underwater Distance Fog
// Hides unloaded chunks: at 96 blocks fog=98%, at 128 blocks fog=99.9%.
// Combined with composite1 sky pixel replacement (depth=1.0 -> waterFogColor),
// chunk boundaries become invisible underwater.
// ============================================================================

// Compute underwater fog factor using CR's squared exponential formula.
// Near-field stays clear (squared term), distant objects fade smoothly.
// Independent of engine fogMode/fogDensity uniforms.
//
// Reference: ComplementaryReimagined lib/atmospherics/fog/waterFog.glsl
float GetUnderwaterFogFactor(float viewDistance)
{
    float dist = viewDistance;

#if WATER_FOG_MULT != 100
    dist *= WATER_FOG_MULT * 0.01;
#endif

    float fog = dist / WATER_UW_FOG_DISTANCE;
    fog       *= fog; // Squared: preserves near-field clarity, dense at distance
    return 1.0 - exp(-fog);
}

// Apply CR-style underwater distance fog to scene color.
// Uses time-dependent fog color (day blue-green, night deep blue).
float3 ApplyCRUnderwaterFog(float3 sceneColor, float viewDistance, float sunVisibility2)
{
    float  fogFactor  = GetUnderwaterFogFactor(viewDistance);
    float3 uwFogColor = lerp(WATER_FOG_COLOR_NIGHT, WATER_FOG_COLOR_DAY, sunVisibility2);
    return lerp(sceneColor, uwFogColor, fogFactor);
}

#endif // LIB_ATMOSPHERE_HLSL
