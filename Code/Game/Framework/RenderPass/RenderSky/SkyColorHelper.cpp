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
// Reference: Iris CelestialUniforms.java:24-32 getSunAngle()
// Uses sunAngle (not celestialAngle) for correct sunrise/sunset timing:
// - tick 18000 (celestialAngle=0.75) -> sunAngle=0.0 -> sunrise
// - tick 0 (celestialAngle=0.0) -> sunAngle=0.25 -> after midnight (no strip)
// - tick 6000 (celestialAngle=0.25) -> sunAngle=0.5 -> sunset
// - tick 12000 (celestialAngle=0.5) -> sunAngle=0.75 -> noon (no strip)
// Peaks at sunAngle 0.0 (sunrise) and 0.5 (sunset)
// Returns zero when sun is high (noon) or low (midnight)
float SkyColorHelper::CalculateSunsetFactor(float sunAngle)
{
    // [FIX] Calculate distance from nearest horizon time (0.0 or 0.5)
    // Sunrise at sunAngle 0.0, Sunset at sunAngle 0.5
    float distToSunrise = std::abs(sunAngle - 0.0f);
    float distToSunset  = std::abs(sunAngle - 0.5f);

    // Use the smaller distance (closer to either sunrise or sunset)
    float distanceFromHorizon = (distToSunrise < distToSunset) ? distToSunrise : distToSunset;

    // [FIX] Horizon window: ±0.1 around 0.0 or 0.5
    // - Sunrise: sunAngle 0.9-1.0 or 0.0-0.1 (wraps around)
    // - Sunset: sunAngle 0.4-0.6
    constexpr float HORIZON_WINDOW = 0.1f;

    if (distanceFromHorizon > HORIZON_WINDOW)
    {
        return 0.0f;
    }

    // Smooth falloff using cosine curve
    float normalizedDistance = distanceFromHorizon / HORIZON_WINDOW;
    float intensity          = std::cos(normalizedDistance * 3.14159f * 0.5f); // cos(0 to PI/2)

    return intensity;
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
// [CONFIGURABLE] Calculate sky color using 4-phase interpolation
// Uses configurable phase colors from s_skyColors (modifiable via ImGui)
//
// [FIX] Phase mapping based on ACTUAL celestialAngle values from TimeOfDayManager::GetCelestialAngle():
// The Minecraft timeOfDay() formula produces NON-LINEAR celestialAngle values:
// - tick 6000  (noon)     -> celestialAngle = 0.0
// - tick 12000 (sunset)   -> celestialAngle ~ 0.215
// - tick 18000 (midnight) -> celestialAngle = 0.5
// - tick 0     (sunrise)  -> celestialAngle ~ 0.785
//
// Phase boundaries adjusted to match actual values:
// - Phase 0 (0.0 - 0.15):   Noon -> Sunset
// - Phase 1 (0.15 - 0.5):   Sunset -> Midnight
// - Phase 2 (0.5 - 0.75):   Midnight -> Sunrise
// - Phase 3 (0.75 - 1.0):   Sunrise -> Noon
Vec3 SkyColorHelper::CalculateSkyColor(float celestialAngle)
{
    // [CONFIGURABLE] Use static phase colors (can be modified via ImGui)
    const Vec3& COLOR_SUNRISE  = s_skyColors.sunrise;
    const Vec3& COLOR_NOON     = s_skyColors.noon;
    const Vec3& COLOR_SUNSET   = s_skyColors.sunset;
    const Vec3& COLOR_MIDNIGHT = s_skyColors.midnight;

    // Normalize celestialAngle to [0, 1)
    float angle = celestialAngle;
    while (angle < 0.0f) angle += 1.0f;
    while (angle >= 1.0f) angle -= 1.0f;

    Vec3 result;

    // [FIX] Phase boundaries based on actual celestialAngle values
    if (angle < 0.15f)
    {
        // Phase 0: Noon (0.0) to Sunset (0.15)
        float t = angle / 0.15f;
        result  = Interpolate(COLOR_NOON, COLOR_SUNSET, t);
    }
    else if (angle < 0.5f)
    {
        // Phase 1: Sunset (0.15) to Midnight (0.5)
        float t = (angle - 0.15f) / 0.35f;
        result  = Interpolate(COLOR_SUNSET, COLOR_MIDNIGHT, t);
    }
    else if (angle < 0.75f)
    {
        // Phase 2: Midnight (0.5) to Sunrise (0.75)
        float t = (angle - 0.5f) / 0.25f;
        result  = Interpolate(COLOR_MIDNIGHT, COLOR_SUNRISE, t);
    }
    else
    {
        // Phase 3: Sunrise (0.75) to Noon (1.0/0.0)
        float t = (angle - 0.75f) / 0.25f;
        result  = Interpolate(COLOR_SUNRISE, COLOR_NOON, t);
    }

    return result;
}

//-----------------------------------------------------------------------------------------------
// [CONFIGURABLE] Calculate fog color using 4-phase interpolation
// Uses configurable phase colors from s_fogColors (modifiable via ImGui)
// Fog color is used for Clear RT and is generally lighter/more desaturated than sky color.
//
// [FIX] Phase mapping based on ACTUAL celestialAngle values from TimeOfDayManager::GetCelestialAngle():
// The Minecraft timeOfDay() formula produces NON-LINEAR celestialAngle values:
// - tick 6000  (noon)     -> celestialAngle = 0.0
// - tick 12000 (sunset)   -> celestialAngle ~ 0.215
// - tick 18000 (midnight) -> celestialAngle = 0.5
// - tick 0     (sunrise)  -> celestialAngle ~ 0.785
//
// Phase boundaries adjusted to match actual values:
// - Phase 0 (0.0 - 0.15):   Noon -> Sunset
// - Phase 1 (0.15 - 0.5):   Sunset -> Midnight
// - Phase 2 (0.5 - 0.75):   Midnight -> Sunrise
// - Phase 3 (0.75 - 1.0):   Sunrise -> Noon
Vec3 SkyColorHelper::CalculateFogColor(float celestialAngle, float sunAngle)
{
    // [CONFIGURABLE] Use static phase colors (can be modified via ImGui)
    const Vec3& FOG_SUNRISE  = s_fogColors.sunrise;
    const Vec3& FOG_NOON     = s_fogColors.noon;
    const Vec3& FOG_SUNSET   = s_fogColors.sunset;
    const Vec3& FOG_MIDNIGHT = s_fogColors.midnight;

    // Normalize celestialAngle to [0, 1)
    float angle = celestialAngle;
    while (angle < 0.0f) angle += 1.0f;
    while (angle >= 1.0f) angle -= 1.0f;

    Vec3 result;

    // [FIX] Phase boundaries based on actual celestialAngle values
    if (angle < 0.15f)
    {
        // Phase 0: Noon (0.0) to Sunset (0.15)
        float t = angle / 0.15f;
        result  = Interpolate(FOG_NOON, FOG_SUNSET, t);
    }
    else if (angle < 0.5f)
    {
        // Phase 1: Sunset (0.15) to Midnight (0.5)
        float t = (angle - 0.15f) / 0.35f;
        result  = Interpolate(FOG_SUNSET, FOG_MIDNIGHT, t);
    }
    else if (angle < 0.75f)
    {
        // Phase 2: Midnight (0.5) to Sunrise (0.75)
        float t = (angle - 0.5f) / 0.25f;
        result  = Interpolate(FOG_MIDNIGHT, FOG_SUNRISE, t);
    }
    else
    {
        // Phase 3: Sunrise (0.75) to Noon (1.0/0.0)
        float t = (angle - 0.75f) / 0.25f;
        result  = Interpolate(FOG_SUNRISE, FOG_NOON, t);
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
