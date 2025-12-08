#include "Game/Framework/RenderPass/RenderSky/SkyColorHelper.hpp"
#include "Engine/Math/MathUtils.hpp"
#include <cmath>

//-----------------------------------------------------------------------------------------------
// [NEW] Static storage for configurable phase colors - initialized with defaults
SkyPhaseColors SkyColorHelper::s_skyColors = SkyPhaseColors::GetDefaultSkyColors();
SkyPhaseColors SkyColorHelper::s_fogColors = SkyPhaseColors::GetDefaultFogColors();

//-----------------------------------------------------------------------------------------------
// [NEW] Calculate daylight factor from celestial angle
// Reference: TimeOfDayManager.cpp:160 CalculateCloudColor()
// Formula: cos(celestialAngle * 2PI) * 2 + 0.5, clamped to [0, 1]
float SkyColorHelper::CalculateDayFactor(float celestialAngle)
{
    constexpr float TWO_PI = 6.2831853f;
    float           h      = std::cos(celestialAngle * TWO_PI) * 2.0f + 0.5f;

    // Clamp to [0, 1]
    if (h < 0.0f) h = 0.0f;
    if (h > 1.0f) h = 1.0f;

    return h;
}

//-----------------------------------------------------------------------------------------------
// [FIX] Calculate sunset/sunrise intensity factor
// Reference: Minecraft DimensionSpecialEffects.java:44-59 getSunriseColor()
// Uses Minecraft's exact formula for sunrise/sunset alpha calculation
//
// Minecraft Algorithm:
// 1. i = cos(timeOfDay * 2π) - 0.0
// 2. Check if i ∈ [-0.4, 0.4] (sunrise/sunset window)
// 3. k = (i - 0.0) / 0.4 * 0.5 + 0.5
// 4. l = 1.0 - (1.0 - sin(k * π)) * 0.99
// 5. alpha = l * l (平方衰减)
//
// Key differences from previous implementation:
// - Uses celestialAngle (timeOfDay), not sunAngle
// - Uses cos-based window check, not distance-based
// - Uses complex sin/cos formula for smooth falloff
// - Applies quadratic decay (l*l)
float SkyColorHelper::CalculateSunsetFactor(float sunAngle)
{
    // [CONVERSION] sunAngle -> celestialAngle
    // Reference: CelestialUniforms.java getSunAngle()
    // sunAngle = celestialAngle + 0.25 (with wrap)
    // Therefore: celestialAngle = sunAngle - 0.25
    float celestialAngle = sunAngle - 0.25f;
    if (celestialAngle < 0.0f) celestialAngle += 1.0f;

    constexpr float TWO_PI = 6.2831855f;
    constexpr float PI     = 3.1415927f;

    // Step 1: i = cos(celestialAngle * 2π) - 0.0
    float i = std::cos(celestialAngle * TWO_PI) - 0.0f;

    // Step 2: Check sunrise/sunset window [-0.4, 0.4]
    constexpr float WINDOW = 0.4f;
    if (i < -WINDOW || i > WINDOW)
    {
        return 0.0f; // Outside sunrise/sunset period
    }

    // Step 3: k = (i - 0.0) / 0.4 * 0.5 + 0.5
    float k = (i - 0.0f) / WINDOW * 0.5f + 0.5f;

    // Step 4: l = 1.0 - (1.0 - sin(k * π)) * 0.99
    float l = 1.0f - (1.0f - std::sin(k * PI)) * 0.99f;

    // Step 5: alpha = l * l (quadratic decay)
    float alpha = l * l;

    return alpha;
}

//-----------------------------------------------------------------------------------------------
// [NEW] Phase color configuration accessors
SkyPhaseColors& SkyColorHelper::GetSkyColors()
{
    return s_skyColors;
}

SkyPhaseColors& SkyColorHelper::GetFogColors()
{
    return s_fogColors;
}

void SkyColorHelper::SetSkyColors(const SkyPhaseColors& colors)
{
    s_skyColors = colors;
}

void SkyColorHelper::SetFogColors(const SkyPhaseColors& colors)
{
    s_fogColors = colors;
}

void SkyColorHelper::ResetSkyColorsToDefault()
{
    s_skyColors = SkyPhaseColors::GetDefaultSkyColors();
}

void SkyColorHelper::ResetFogColorsToDefault()
{
    s_fogColors = SkyPhaseColors::GetDefaultFogColors();
}

//-----------------------------------------------------------------------------------------------
// [CONFIGURABLE] Calculate sky color using 5-phase interpolation
// Uses configurable phase colors from s_skyColors (modifiable via ImGui)
//
// [NEW] 5-phase system with Dawn phase for tick 0-1000 transition
// The Minecraft timeOfDay() formula produces NON-LINEAR celestialAngle values:
// - tick 6000  (noon)     -> celestialAngle = 0.0
// - tick 12000 (sunset)   -> celestialAngle ~ 0.25
// - tick 18000 (midnight) -> celestialAngle = 0.5
// - tick 0     (sunrise)  -> celestialAngle ~ 0.75
// - tick 1000  (dawn)     -> celestialAngle ~ 0.79 [NEW]
//
// Phase boundaries (5-phase):
// - Phase 0 (0.0 - 0.25):   Noon -> Sunset
// - Phase 1 (0.25 - 0.5):   Sunset -> Midnight
// - Phase 2 (0.5 - 0.75):   Midnight -> Sunrise
// - Phase 3 (0.75 - 0.79):  Sunrise -> Dawn [NEW]
// - Phase 4 (0.79 - 1.0):   Dawn -> Noon
Vec3 SkyColorHelper::CalculateSkyColor(float celestialAngle)
{
    // [CONFIGURABLE] Use static phase colors (can be modified via ImGui)
    const Vec3& COLOR_SUNRISE  = s_skyColors.sunrise;
    const Vec3& COLOR_DAWN     = s_skyColors.dawn; // [NEW] Light yellow morning
    const Vec3& COLOR_NOON     = s_skyColors.noon;
    const Vec3& COLOR_SUNSET   = s_skyColors.sunset;
    const Vec3& COLOR_MIDNIGHT = s_skyColors.midnight;

    // Normalize celestialAngle to [0, 1)
    float angle = celestialAngle;
    while (angle < 0.0f) angle += 1.0f;
    while (angle >= 1.0f) angle -= 1.0f;

    Vec3 result;

    // [NEW] 5-phase interpolation with Dawn phase
    if (angle < 0.25f)
    {
        // Phase 0: Noon (0.0) to Sunset (0.25)
        float t = angle / 0.25f;
        result  = Interpolate(COLOR_NOON, COLOR_SUNSET, t);
    }
    else if (angle < 0.5f)
    {
        // Phase 1: Sunset (0.25) to Midnight (0.5)
        float t = (angle - 0.25f) / 0.25f;
        result  = Interpolate(COLOR_SUNSET, COLOR_MIDNIGHT, t);
    }
    else if (angle < 0.75f)
    {
        // Phase 2: Midnight (0.5) to Sunrise (0.75)
        float t = (angle - 0.5f) / 0.25f;
        result  = Interpolate(COLOR_MIDNIGHT, COLOR_SUNRISE, t);
    }
    else if (angle < 0.79f)
    {
        // Phase 3: Sunrise (0.75) to Dawn (0.79) [NEW]
        // This is the orange->yellow transition (tick 0 -> tick 1000)
        float t = (angle - 0.75f) / 0.04f;
        result  = Interpolate(COLOR_SUNRISE, COLOR_DAWN, t);
    }
    else
    {
        // Phase 4: Dawn (0.79) to Noon (1.0/0.0)
        // Morning yellow to bright blue transition
        float t = (angle - 0.79f) / 0.21f;
        result  = Interpolate(COLOR_DAWN, COLOR_NOON, t);
    }

    return result;
}

//-----------------------------------------------------------------------------------------------
// [CONFIGURABLE] Calculate fog color using 5-phase interpolation
// Uses configurable phase colors from s_fogColors (modifiable via ImGui)
// Fog color is used for Clear RT and is generally lighter/more desaturated than sky color.
//
// [NEW] 5-phase system with Dawn phase for tick 0-1000 transition
// Phase boundaries (5-phase, same as CalculateSkyColor):
// - Phase 0 (0.0 - 0.25):   Noon -> Sunset
// - Phase 1 (0.25 - 0.5):   Sunset -> Midnight
// - Phase 2 (0.5 - 0.75):   Midnight -> Sunrise
// - Phase 3 (0.75 - 0.79):  Sunrise -> Dawn [NEW]
// - Phase 4 (0.79 - 1.0):   Dawn -> Noon
Vec3 SkyColorHelper::CalculateFogColor(float celestialAngle, float sunAngle)
{
    // [CONFIGURABLE] Use static phase colors (can be modified via ImGui)
    const Vec3& FOG_SUNRISE  = s_fogColors.sunrise;
    const Vec3& FOG_DAWN     = s_fogColors.dawn; // [NEW] Light morning fog
    const Vec3& FOG_NOON     = s_fogColors.noon;
    const Vec3& FOG_SUNSET   = s_fogColors.sunset;
    const Vec3& FOG_MIDNIGHT = s_fogColors.midnight;

    // Normalize celestialAngle to [0, 1)
    float angle = celestialAngle;
    while (angle < 0.0f) angle += 1.0f;
    while (angle >= 1.0f) angle -= 1.0f;

    Vec3 result;

    // [NEW] 5-phase interpolation with Dawn phase
    if (angle < 0.25f)
    {
        // Phase 0: Noon (0.0) to Sunset (0.25)
        float t = angle / 0.25f;
        result  = Interpolate(FOG_NOON, FOG_SUNSET, t);
    }
    else if (angle < 0.5f)
    {
        // Phase 1: Sunset (0.25) to Midnight (0.5)
        float t = (angle - 0.25f) / 0.25f;
        result  = Interpolate(FOG_SUNSET, FOG_MIDNIGHT, t);
    }
    else if (angle < 0.75f)
    {
        // Phase 2: Midnight (0.5) to Sunrise (0.75)
        float t = (angle - 0.5f) / 0.25f;
        result  = Interpolate(FOG_MIDNIGHT, FOG_SUNRISE, t);
    }
    else if (angle < 0.79f)
    {
        // Phase 3: Sunrise (0.75) to Dawn (0.79) [NEW]
        float t = (angle - 0.75f) / 0.04f;
        result  = Interpolate(FOG_SUNRISE, FOG_DAWN, t);
    }
    else
    {
        // Phase 4: Dawn (0.79) to Noon (1.0/0.0)
        float t = (angle - 0.79f) / 0.21f;
        result  = Interpolate(FOG_DAWN, FOG_NOON, t);
    }

    // sunAngle parameter is kept for API compatibility but not used in configurable mode
    (void)sunAngle;

    return result;
}

//-----------------------------------------------------------------------------------------------
// [FIX] Calculate sunrise/sunset glow color
// Returns RGBA where RGB is orange tint and A is intensity
// Alpha peaks at sunrise/sunset (sunAngle 0.0/0.5), zero otherwise
Vec4 SkyColorHelper::CalculateSunriseColor(float sunAngle)
{
    const Vec3 SUNRISE_COLOR(1.0f, 0.4f, 0.2f); // Orange-red glow (const - Vec3 not constexpr)

    float intensity = CalculateSunsetFactor(sunAngle);

    return Vec4(SUNRISE_COLOR.x, SUNRISE_COLOR.y, SUNRISE_COLOR.z, intensity);
}

//-----------------------------------------------------------------------------------------------
// [NEW] Calculate sky color with CPU-side fog blending based on elevation angle
// Reference: Iris FogMode.OFF for SKY_BASIC - skyColor is pre-blended with fog
// Reference: Minecraft ClientLevel.getSkyColor() considers view direction
//
// Algorithm:
// 1. Get base skyColor and fogColor for current celestialAngle
// 2. Calculate elevationFactor from elevation angle (0° horizon -> 90° zenith)
// 3. Apply smooth falloff: factor = pow(sin(elevation), 0.5)
// 4. Blend: result = lerp(fogColor, skyColor, factor)
//
// This creates the smooth gradient from fogColor at horizon to skyColor at zenith
Vec3 SkyColorHelper::CalculateSkyColorWithFog(float celestialAngle, float elevationDegrees)
{
    // Step 1: Get base colors for current time
    Vec3 skyColor = CalculateSkyColor(celestialAngle);
    Vec3 fogColor = CalculateFogColor(celestialAngle, 0.0f); // sunAngle not used in current impl

    // Step 2: Clamp elevation to valid range [0, 90] for sky dome
    // Negative elevations (void dome) handled separately
    float clampedElevation = elevationDegrees;
    if (clampedElevation < 0.0f) clampedElevation = 0.0f;
    if (clampedElevation > 90.0f) clampedElevation = 90.0f;

    // Step 3: Calculate elevation factor with smooth falloff
    // Using sin(elevation) creates natural falloff at horizon
    // Square root makes the transition more gradual near horizon
    constexpr float DEG_TO_RAD   = 0.017453292f;
    float           elevationRad = clampedElevation * DEG_TO_RAD;
    float           sinElevation = std::sin(elevationRad);

    // Smooth falloff: sqrt gives more gradual transition near horizon
    // At 0°: factor = 0 (pure fog)
    // At 30°: factor ≈ 0.71
    // At 60°: factor ≈ 0.93
    // At 90°: factor = 1 (pure sky)
    float elevationFactor = std::sqrt(sinElevation);

    // Step 4: Blend fogColor -> skyColor based on elevation
    // lerp(fogColor, skyColor, factor) = fogColor + (skyColor - fogColor) * factor
    Vec3 result;
    result.x = fogColor.x + (skyColor.x - fogColor.x) * elevationFactor;
    result.y = fogColor.y + (skyColor.y - fogColor.y) * elevationFactor;
    result.z = fogColor.z + (skyColor.z - fogColor.z) * elevationFactor;

    return result;
}

//-----------------------------------------------------------------------------------------------
// [NEW] Convert vertex position to elevation angle
// Used for sky dome vertex color calculation
//
// Coordinate system (Engine Z-up):
// - Z axis = vertical (up)
// - XY plane = horizontal
// - Elevation = angle from horizontal plane to vertex direction
//
// Formula: elevation = atan2(z, horizontal_distance) * RAD_TO_DEG
// - At zenith (0,0,16): elevation = 90°
// - At horizon (512,0,0): elevation = 0°
float SkyColorHelper::CalculateElevationAngle(const Vec3& vertexPos)
{
    constexpr float RAD_TO_DEG = 57.29577951f;

    // Calculate horizontal distance from origin
    float horizontalDist = std::sqrt(vertexPos.x * vertexPos.x + vertexPos.y * vertexPos.y);

    // Calculate elevation angle using atan2
    // atan2(z, horizontal) gives angle from horizontal plane
    float elevationRad = std::atan2(vertexPos.z, horizontalDist);

    return elevationRad * RAD_TO_DEG;
}
