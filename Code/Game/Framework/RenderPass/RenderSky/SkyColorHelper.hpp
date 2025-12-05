#pragma once
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/Vec4.hpp"

//-----------------------------------------------------------------------------------------------
/**
 * @brief Static helper class for calculating sky colors (Minecraft vanilla style)
 * 
 * This class provides utility functions to calculate sky and sunrise colors based on
 * sunAngle (not celestialAngle), following Minecraft's vanilla sky rendering algorithms.
 * All methods are static and the class cannot be instantiated.
 * 
 * @note Algorithm Reference: Minecraft ClientLevel.java getSkyColor() and getSunriseColor()
 * @note Reference: Iris CelestialUniforms.java:24-32 getSunAngle()
 * @note Sun Angle: sunAngle = celestialAngle + 0.25 (with wrap-around)
 *       - sunAngle 0.0 = sunrise (tick 18000, celestialAngle 0.75)
 *       - sunAngle 0.25 = after midnight (tick 0, celestialAngle 0.0)
 *       - sunAngle 0.5 = sunset (tick 6000, celestialAngle 0.25)
 *       - sunAngle 0.75 = noon (tick 12000, celestialAngle 0.5)
 */
class SkyColorHelper
{
public:
    // [REMOVED] Static-only class - no instantiation allowed
    SkyColorHelper()                                 = delete;
    SkyColorHelper(const SkyColorHelper&)            = delete;
    SkyColorHelper& operator=(const SkyColorHelper&) = delete;

    //-----------------------------------------------------------------------------------------------
    /**
     * @brief Calculate sky color based on sunAngle (not celestialAngle)
     * 
     * Computes the base sky color using Minecraft's vanilla algorithm. The color transitions
     * from dark blue at night to bright blue during day, with smooth gradients at sunrise/sunset.
     * 
     * @param sunAngle Current sun angle (0.0 - 1.0), where sunAngle = celestialAngle + 0.25
     * @return Vec3 RGB color values (0.0 - 1.0 range)
     * 
     * @note Algorithm:
     *       1. Calculate day factor from sun angle
     *       2. Base color: (0.26, 0.5, 1.0) - light blue
     *       3. Apply day factor to darken at night
     *       4. Clamp to valid range [0, 1]
     * 
     * @note Reference: Minecraft ClientLevel.java:656-671 getSkyColor()
     * @note Reference: Iris CelestialUniforms.java:24-32 getSunAngle()
     */
    static Vec3 CalculateSkyColor(float sunAngle);

    /**
     * @brief Calculate sunrise/sunset glow color based on sunAngle (not celestialAngle)
     * 
     * Computes the orange glow color and alpha for sunrise/sunset rendering. Returns
     * zero alpha during day and night, with peak intensity at horizon times.
     * 
     * @param sunAngle Current sun angle (0.0 - 1.0), where sunAngle = celestialAngle + 0.25
     * @return Vec4 RGBA color (RGB: orange tint, A: intensity 0.0-1.0)
     * 
     * @note Algorithm:
     *       1. Calculate sunset factor (peaks at sunrise/sunset)
     *       2. Base color: orange (1.0, 0.5, 0.0)
     *       3. Alpha represents glow intensity
     *       4. Returns (0,0,0,0) when sun is high or low
     * 
     * @note Reference: Minecraft LevelRenderer.java renderSky() sunrise color calculation
     * @note Reference: Iris CelestialUniforms.java:24-32 getSunAngle()
     */
    static Vec4 CalculateSunriseColor(float sunAngle);

private:
    //-----------------------------------------------------------------------------------------------
    /**
     * @brief Calculate daylight factor from sunAngle (not celestialAngle)
     * 
     * Computes a factor representing daylight intensity (1.0 = full day, 0.0 = full night).
     * Uses cosine function to create smooth day/night transitions.
     * 
     * @param sunAngle Current sun angle (0.0 - 1.0), where sunAngle = celestialAngle + 0.25
     * @return float Daylight factor (0.0 - 1.0, clamped)
     * 
     * @note Formula: cos(sunAngle * 2PI) * 2 + 0.5, clamped to [0, 1]
     * @note Same algorithm as TimeOfDayManager::CalculateCloudColor()
     */
    static float CalculateDayFactor(float sunAngle);

    /**
     * @brief Calculate sunset/sunrise intensity factor
     * 
     * Computes a factor representing sunrise/sunset glow intensity. Peaks at horizon
     * times (sunAngle near 0.0 or 0.5) and returns zero when sun is high or low.
     * 
     * @param sunAngle Current sun angle (0.0 - 1.0), where sunAngle = celestialAngle + 0.25
     * @return float Sunset factor (0.0 - 1.0)
     * 
     * @note Returns non-zero only when sun is near horizon (±30 degrees)
     * @note Used to modulate sunrise/sunset glow alpha channel
     * @note Reference: Iris CelestialUniforms.java:24-32 getSunAngle()
     */
    static float CalculateSunsetFactor(float sunAngle);
};
