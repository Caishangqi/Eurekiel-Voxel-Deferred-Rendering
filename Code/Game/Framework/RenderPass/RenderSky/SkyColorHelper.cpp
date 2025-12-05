#include "Game/Framework/RenderPass/RenderSky/SkyColorHelper.hpp"
#include "Engine/Math/MathUtils.hpp"
#include <cmath>

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
// [FIX] Calculate sky color based on sunAngle (not celestialAngle)
// Algorithm:
// 1. Calculate day factor and sunset factor
// 2. Interpolate between day/night colors based on day factor
// 3. Blend in sunset color based on sunset factor
Vec3 SkyColorHelper::CalculateSkyColor(float sunAngle)
{
    // Color constants (use const instead of constexpr - Vec3 constructor is not constexpr)
    const Vec3 DAY_SKY_COLOR(0.47f, 0.65f, 1.0f); // Bright blue
    const Vec3 NIGHT_SKY_COLOR(0.0f, 0.0f, 0.02f); // Very dark blue
    const Vec3 SUNSET_COLOR(1.0f, 0.5f, 0.2f); // Orange

    // Calculate factors using sunAngle
    float dayFactor    = CalculateDayFactor(sunAngle);
    float sunsetFactor = CalculateSunsetFactor(sunAngle);

    // Interpolate between night and day colors
    Vec3 baseColor = Interpolate(NIGHT_SKY_COLOR, DAY_SKY_COLOR, dayFactor);

    // Blend in sunset color at horizon times
    Vec3 finalColor = Interpolate(baseColor, SUNSET_COLOR, sunsetFactor * 0.6f);

    return finalColor;
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
