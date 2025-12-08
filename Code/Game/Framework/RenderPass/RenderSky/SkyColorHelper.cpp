#include "Game/Framework/RenderPass/RenderSky/SkyColorHelper.hpp"
#include "Engine/Math/MathUtils.hpp"
#include <cmath>

//-----------------------------------------------------------------------------------------------
// [NEW] BezierEasing::Apply implementation
// Uses cubic Bezier evaluation for time remapping (like CSS cubic-bezier)
//
// The curve is defined by 4 points:
// P0 = (0, 0), P1 = (x1, y1), P2 = (x2, y2), P3 = (1, 1)
//
// For time remapping, we need to find the Y value for a given X (the linear t).
// This requires solving the cubic Bezier equation for the parameter that gives us X,
// then evaluating Y at that parameter.
//
// Simplified approach: Since we're mapping t to eased-t, we can use direct evaluation
// with the assumption that the curve is monotonic (always increasing in X).
float BezierEasing::Apply(float t) const
{
    // Clamp input to [0, 1]
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;

    // For a proper CSS cubic-bezier, we need to find the parameter 'u' such that
    // the X coordinate of the Bezier curve equals 't', then return the Y coordinate.
    //
    // Bezier X(u) = 3(1-u)^2 * u * x1 + 3(1-u) * u^2 * x2 + u^3
    // Bezier Y(u) = 3(1-u)^2 * u * y1 + 3(1-u) * u^2 * y2 + u^3
    //
    // We use Newton-Raphson iteration to solve for u given X(u) = t

    float u = t; // Initial guess

    // Newton-Raphson iteration (typically converges in 4-8 iterations)
    for (int i = 0; i < 8; ++i)
    {
        float u2         = u * u;
        float u3         = u2 * u;
        float oneMinusU  = 1.0f - u;
        float oneMinusU2 = oneMinusU * oneMinusU;

        // Calculate X(u)
        float xu = 3.0f * oneMinusU2 * u * p1.x + 3.0f * oneMinusU * u2 * p2.x + u3;

        // Calculate X'(u) for Newton-Raphson
        float dxu = 3.0f * oneMinusU2 * p1.x + 6.0f * oneMinusU * u * (p2.x - p1.x) + 3.0f * u2 * (1.0f - p2.x);

        // Avoid division by zero
        if (std::abs(dxu) < 1e-6f) break;

        // Newton-Raphson step
        float diff = xu - t;
        if (std::abs(diff) < 1e-6f) break; // Close enough

        u = u - diff / dxu;

        // Clamp u to [0, 1]
        if (u < 0.0f) u = 0.0f;
        if (u > 1.0f) u = 1.0f;
    }

    // Calculate Y(u) with the found parameter
    float u2         = u * u;
    float u3         = u2 * u;
    float oneMinusU  = 1.0f - u;
    float oneMinusU2 = oneMinusU * oneMinusU;

    float yu = 3.0f * oneMinusU2 * u * p1.y + 3.0f * oneMinusU * u2 * p2.y + u3;

    return yu;
}

//-----------------------------------------------------------------------------------------------
// Static storage for configurable phase colors and easing - initialized with defaults
SkyPhaseColors     SkyColorHelper::s_skyColors    = SkyPhaseColors::GetDefaultSkyColors();
SkyPhaseColors     SkyColorHelper::s_fogColors    = SkyPhaseColors::GetDefaultFogColors();
SunriseStripColors SkyColorHelper::s_stripColors  = SunriseStripColors::GetDefault();
SkyEasingConfig    SkyColorHelper::s_easingConfig = SkyEasingConfig::GetDefault();

//-----------------------------------------------------------------------------------------------
// Calculate daylight factor from celestial angle
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
// Calculate sunset/sunrise intensity factor
// Reference: Minecraft DimensionSpecialEffects.java:44-59 getSunriseColor()
float SkyColorHelper::CalculateSunsetFactor(float sunAngle)
{
    // [CONVERSION] sunAngle -> celestialAngle
    float celestialAngle = sunAngle - 0.25f;
    if (celestialAngle < 0.0f) celestialAngle += 1.0f;

    constexpr float TWO_PI = 6.2831855f;
    constexpr float PI     = 3.1415927f;

    // Step 1: i = cos(celestialAngle * 2pi) - 0.0
    float i = std::cos(celestialAngle * TWO_PI) - 0.0f;

    // Step 2: Check sunrise/sunset window [-0.4, 0.4]
    constexpr float WINDOW = 0.4f;
    if (i < -WINDOW || i > WINDOW)
    {
        return 0.0f; // Outside sunrise/sunset period
    }

    // Step 3: k = (i - 0.0) / 0.4 * 0.5 + 0.5
    float k = (i - 0.0f) / WINDOW * 0.5f + 0.5f;

    // Step 4: l = 1.0 - (1.0 - sin(k * pi)) * 0.99
    float l = 1.0f - (1.0f - std::sin(k * PI)) * 0.99f;

    // Step 5: alpha = l * l (quadratic decay)
    float alpha = l * l;

    return alpha;
}

//-----------------------------------------------------------------------------------------------
// Phase color configuration accessors
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

// Sunrise strip color accessors
SunriseStripColors& SkyColorHelper::GetStripColors()
{
    return s_stripColors;
}

void SkyColorHelper::SetStripColors(const SunriseStripColors& colors)
{
    s_stripColors = colors;
}

void SkyColorHelper::ResetStripColorsToDefault()
{
    s_stripColors = SunriseStripColors::GetDefault();
}

// [NEW] Easing configuration accessors
SkyEasingConfig& SkyColorHelper::GetEasingConfig()
{
    return s_easingConfig;
}

void SkyColorHelper::SetEasingConfig(const SkyEasingConfig& config)
{
    s_easingConfig = config;
}

void SkyColorHelper::ResetEasingToDefault()
{
    s_easingConfig = SkyEasingConfig::GetDefault();
}

void SkyColorHelper::SetMinecraftStyleEasing()
{
    s_easingConfig = SkyEasingConfig::GetMinecraftStyle();
}

//-----------------------------------------------------------------------------------------------
// [NEW] Calculate sky color using 5-phase interpolation with Bezier easing
//
// Phase boundaries (5-phase):
// - Phase 0 (0.0 - 0.25):   Noon -> Sunset
// - Phase 1 (0.25 - 0.5):   Sunset -> Midnight
// - Phase 2 (0.5 - 0.75):   Midnight -> Sunrise
// - Phase 3 (0.75 - 0.79):  Sunrise -> Dawn
// - Phase 4 (0.79 - 1.0):   Dawn -> Noon
Vec3 SkyColorHelper::CalculateSkyColor(float celestialAngle)
{
    // Use static phase colors (can be modified via ImGui)
    const Vec3& COLOR_SUNRISE  = s_skyColors.sunrise;
    const Vec3& COLOR_DAWN     = s_skyColors.dawn;
    const Vec3& COLOR_NOON     = s_skyColors.noon;
    const Vec3& COLOR_SUNSET   = s_skyColors.sunset;
    const Vec3& COLOR_MIDNIGHT = s_skyColors.midnight;

    // Normalize celestialAngle to [0, 1)
    float angle = celestialAngle;
    while (angle < 0.0f) angle += 1.0f;
    while (angle >= 1.0f) angle -= 1.0f;

    Vec3 result;

    // 5-phase interpolation with Bezier easing
    if (angle < 0.25f)
    {
        // Phase 0: Noon (0.0) to Sunset (0.25)
        float t      = angle / 0.25f;
        float easedT = s_easingConfig.noonToSunset.Apply(t);
        result       = Interpolate(COLOR_NOON, COLOR_SUNSET, easedT);
    }
    else if (angle < 0.5f)
    {
        // Phase 1: Sunset (0.25) to Midnight (0.5)
        float t      = (angle - 0.25f) / 0.25f;
        float easedT = s_easingConfig.sunsetToMidnight.Apply(t);
        result       = Interpolate(COLOR_SUNSET, COLOR_MIDNIGHT, easedT);
    }
    else if (angle < 0.75f)
    {
        // Phase 2: Midnight (0.5) to Sunrise (0.75)
        float t      = (angle - 0.5f) / 0.25f;
        float easedT = s_easingConfig.midnightToSunrise.Apply(t);
        result       = Interpolate(COLOR_MIDNIGHT, COLOR_SUNRISE, easedT);
    }
    else if (angle < 0.79f)
    {
        // Phase 3: Sunrise (0.75) to Dawn (0.79)
        float t      = (angle - 0.75f) / 0.04f;
        float easedT = s_easingConfig.sunriseToDawn.Apply(t);
        result       = Interpolate(COLOR_SUNRISE, COLOR_DAWN, easedT);
    }
    else
    {
        // Phase 4: Dawn (0.79) to Noon (1.0/0.0)
        float t      = (angle - 0.79f) / 0.21f;
        float easedT = s_easingConfig.dawnToNoon.Apply(t);
        result       = Interpolate(COLOR_DAWN, COLOR_NOON, easedT);
    }

    return result;
}

//-----------------------------------------------------------------------------------------------
// [NEW] Calculate fog color using 5-phase interpolation with Bezier easing
Vec3 SkyColorHelper::CalculateFogColor(float celestialAngle, float sunAngle)
{
    // Use static phase colors (can be modified via ImGui)
    const Vec3& FOG_SUNRISE  = s_fogColors.sunrise;
    const Vec3& FOG_DAWN     = s_fogColors.dawn;
    const Vec3& FOG_NOON     = s_fogColors.noon;
    const Vec3& FOG_SUNSET   = s_fogColors.sunset;
    const Vec3& FOG_MIDNIGHT = s_fogColors.midnight;

    // Normalize celestialAngle to [0, 1)
    float angle = celestialAngle;
    while (angle < 0.0f) angle += 1.0f;
    while (angle >= 1.0f) angle -= 1.0f;

    Vec3 result;

    // 5-phase interpolation with Bezier easing (same phases as CalculateSkyColor)
    if (angle < 0.25f)
    {
        // Phase 0: Noon (0.0) to Sunset (0.25)
        float t      = angle / 0.25f;
        float easedT = s_easingConfig.noonToSunset.Apply(t);
        result       = Interpolate(FOG_NOON, FOG_SUNSET, easedT);
    }
    else if (angle < 0.5f)
    {
        // Phase 1: Sunset (0.25) to Midnight (0.5)
        float t      = (angle - 0.25f) / 0.25f;
        float easedT = s_easingConfig.sunsetToMidnight.Apply(t);
        result       = Interpolate(FOG_SUNSET, FOG_MIDNIGHT, easedT);
    }
    else if (angle < 0.75f)
    {
        // Phase 2: Midnight (0.5) to Sunrise (0.75)
        float t      = (angle - 0.5f) / 0.25f;
        float easedT = s_easingConfig.midnightToSunrise.Apply(t);
        result       = Interpolate(FOG_MIDNIGHT, FOG_SUNRISE, easedT);
    }
    else if (angle < 0.79f)
    {
        // Phase 3: Sunrise (0.75) to Dawn (0.79)
        float t      = (angle - 0.75f) / 0.04f;
        float easedT = s_easingConfig.sunriseToDawn.Apply(t);
        result       = Interpolate(FOG_SUNRISE, FOG_DAWN, easedT);
    }
    else
    {
        // Phase 4: Dawn (0.79) to Noon (1.0/0.0)
        float t      = (angle - 0.79f) / 0.21f;
        float easedT = s_easingConfig.dawnToNoon.Apply(t);
        result       = Interpolate(FOG_DAWN, FOG_NOON, easedT);
    }

    // sunAngle parameter is kept for API compatibility but not used
    (void)sunAngle;

    return result;
}

//-----------------------------------------------------------------------------------------------
// Calculate sunrise/sunset glow color using configurable strip colors
Vec4 SkyColorHelper::CalculateSunriseColor(float sunAngle)
{
    float intensity = CalculateSunsetFactor(sunAngle);

    // Early exit if no glow visible
    if (intensity < 0.001f)
    {
        return Vec4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    // Determine if we're in sunrise or sunset based on sunAngle
    float distToSunrise = sunAngle < 0.5f ? sunAngle : (1.0f - sunAngle);
    float distToSunset  = sunAngle > 0.5f ? (sunAngle - 0.5f) : (0.5f - sunAngle);

    Vec3 stripColor;
    if (distToSunrise < distToSunset)
    {
        // Closer to sunrise (sunAngle near 0.0 or 1.0)
        stripColor = s_stripColors.sunriseStrip;
    }
    else
    {
        // Closer to sunset (sunAngle near 0.5)
        stripColor = s_stripColors.sunsetStrip;
    }

    return Vec4(stripColor.x, stripColor.y, stripColor.z, intensity);
}

//-----------------------------------------------------------------------------------------------
// Calculate sky color with CPU-side fog blending based on elevation angle
Vec3 SkyColorHelper::CalculateSkyColorWithFog(float celestialAngle, float elevationDegrees)
{
    // Step 1: Get base colors for current time
    Vec3 skyColor = CalculateSkyColor(celestialAngle);
    Vec3 fogColor = CalculateFogColor(celestialAngle, 0.0f);

    // Step 2: Clamp elevation to valid range [0, 90] for sky dome
    float clampedElevation = elevationDegrees;
    if (clampedElevation < 0.0f) clampedElevation = 0.0f;
    if (clampedElevation > 90.0f) clampedElevation = 90.0f;

    // Step 3: Calculate elevation factor with smooth falloff
    constexpr float DEG_TO_RAD   = 0.017453292f;
    float           elevationRad = clampedElevation * DEG_TO_RAD;
    float           sinElevation = std::sin(elevationRad);

    // Smooth falloff: sqrt gives more gradual transition near horizon
    float elevationFactor = std::sqrt(sinElevation);

    // Step 4: Blend fogColor -> skyColor based on elevation
    Vec3 result;
    result.x = fogColor.x + (skyColor.x - fogColor.x) * elevationFactor;
    result.y = fogColor.y + (skyColor.y - fogColor.y) * elevationFactor;
    result.z = fogColor.z + (skyColor.z - fogColor.z) * elevationFactor;

    return result;
}

//-----------------------------------------------------------------------------------------------
// Convert vertex position to elevation angle
float SkyColorHelper::CalculateElevationAngle(const Vec3& vertexPos)
{
    constexpr float RAD_TO_DEG = 57.29577951f;

    // Calculate horizontal distance from origin
    float horizontalDist = std::sqrt(vertexPos.x * vertexPos.x + vertexPos.y * vertexPos.y);

    // Calculate elevation angle using atan2
    float elevationRad = std::atan2(vertexPos.z, horizontalDist);

    return elevationRad * RAD_TO_DEG;
}
