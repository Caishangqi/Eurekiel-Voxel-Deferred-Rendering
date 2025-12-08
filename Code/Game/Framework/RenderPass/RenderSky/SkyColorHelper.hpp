#pragma once
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/Vec4.hpp"

//-----------------------------------------------------------------------------------------------
/**
 * @brief Configuration structure for 5-phase sky colors
 * 
 * Stores the 5 key colors used for sky and fog interpolation:
 * - Sunrise (celestialAngle ~0.785, tick 0, 6:00 AM) - Orange sunrise glow
 * - Dawn (celestialAngle ~0.83, tick 1000, 7:00 AM) - Light yellow morning
 * - Noon (celestialAngle 0.0, tick 6000, 12:00 PM) - Bright blue
 * - Sunset (celestialAngle ~0.215, tick 12000, 6:00 PM) - Orange-red
 * - Midnight (celestialAngle 0.5, tick 18000, 12:00 AM) - Dark blue/black
 * 
 * @note [NEW] Dawn phase added to support tick 0-1000 transition
 *       Minecraft shows light yellow sky during early morning (tick 1000+)
 *       This allows sunrise strip to be orange while sky transitions to yellow
 */
struct SkyPhaseColors
{
    Vec3 sunrise; // tick 0 (6:00 AM) - Orange-pink sunrise
    Vec3 dawn; // tick 1000 (7:00 AM) - Light yellow morning [NEW]
    Vec3 noon; // tick 6000 (12:00 PM) - Bright blue
    Vec3 sunset; // tick 13000 (6:00 PM) - Orange-red sunset
    Vec3 midnight; // tick 18000 (12:00 AM) - Dark blue/black

    // Default sky dome colors (Minecraft-inspired)
    static SkyPhaseColors GetDefaultSkyColors()
    {
        SkyPhaseColors colors;
        colors.sunrise  = Vec3(0.5f, 0.62f, 0.87f); // Orange-pink sunrise
        colors.dawn     = Vec3(0.52f, 0.69f, 1.0f); // Light yellow/pale blue morning [NEW]
        colors.noon     = Vec3(0.51f, 0.68f, 1.0f); // Bright blue
        colors.sunset   = Vec3(0.26f, 0.26f, 0.35f); // Orange-red sunset
        colors.midnight = Vec3(0.0f, 0.01f, 0.01f); // Near black
        return colors;
    }

    // Default fog colors (lighter/more desaturated)
    static SkyPhaseColors GetDefaultFogColors()
    {
        SkyPhaseColors colors;
        colors.sunrise  = Vec3(0.75f, 0.69f, 0.65f); // Warm sunrise fog
        colors.dawn     = Vec3(0.71f, 0.82f, 1.0f); // Light morning fog [NEW]
        colors.noon     = Vec3(0.71f, 0.82f, 1.0f); // Bright day fog
        colors.sunset   = Vec3(0.73f, 0.31f, 0.24f); // Warm sunset fog
        colors.midnight = Vec3(0.04f, 0.04f, 0.07f); // Dark night fog
        return colors;
    }
};

//-----------------------------------------------------------------------------------------------
/**
 * @brief Static helper class for calculating sky colors (4-phase interpolation)
 * 
 * This class provides utility functions to calculate sky, fog, and sunrise colors
 * using a simple 4-phase interpolation system. Instead of complex Minecraft biome
 * calculations, we define 4 key colors (Sunrise, Noon, Sunset, Midnight) and
 * interpolate between them based on celestialAngle.
 * 
 * All methods are static and the class cannot be instantiated.
 * 
 * @note Phase Mapping (based on celestialAngle from TimeOfDayManager::GetCelestialAngle()):
 *       - celestialAngle 0.0  = tick 0     = 6:00 AM  (Sunrise/Morning)
 *       - celestialAngle 0.25 = tick 6000  = 12:00 PM (Noon, sun at zenith)
 *       - celestialAngle 0.5  = tick 12000 = 6:00 PM  (Sunset)
 *       - celestialAngle 0.75 = tick 18000 = 12:00 AM (Midnight, moon at zenith)
 * 
 * @note Sky Colors (4 key points):
 *       - Sunrise:  rgb(242, 140, 89)  - Orange-pink
 *       - Noon:     rgb(120, 168, 255) - Bright blue
 *       - Sunset:   rgb(242, 115, 64)  - Orange-red
 *       - Midnight: rgb(13, 13, 31)    - Dark blue
 * 
 * @note Reference: Iris CelestialUniforms.java:24-32 getSunAngle()
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
     * @brief Calculate sky color using 4-phase interpolation
     * 
     * Computes the sky dome color by interpolating between 4 key colors based on celestialAngle.
     * This provides smooth transitions throughout the day/night cycle.
     * 
     * @param celestialAngle Current celestial angle (0.0 - 1.0), from TimeOfDayManager
     * @return Vec3 RGB color values (0.0 - 1.0 range)
     * 
     * @note 4-Phase Interpolation:
     *       - Phase 0 (0.0-0.25):  Sunrise → Noon
     *       - Phase 1 (0.25-0.5):  Noon → Sunset
     *       - Phase 2 (0.5-0.75):  Sunset → Midnight
     *       - Phase 3 (0.75-1.0):  Midnight → Sunrise
     */
    static Vec3 CalculateSkyColor(float celestialAngle);

    /**
     * @brief Calculate fog color using 4-phase interpolation
     * 
     * Computes the fog color used for clearing the render target (Clear RT).
     * Fog colors are lighter/more desaturated than sky colors to create atmospheric depth.
     * 
     * @param celestialAngle Current celestial angle (0.0 - 1.0)
     * @param sunAngle Kept for API compatibility (not used in simple mode)
     * @return Vec3 RGB fog color (0.0 - 1.0 range)
     * 
     * @note 4-Phase Interpolation (same phases as CalculateSkyColor):
     *       - Phase 0 (0.0-0.25):  Sunrise fog → Noon fog
     *       - Phase 1 (0.25-0.5):  Noon fog → Sunset fog
     *       - Phase 2 (0.5-0.75):  Sunset fog → Midnight fog
     *       - Phase 3 (0.75-1.0):  Midnight fog → Sunrise fog
     */
    static Vec3 CalculateFogColor(float celestialAngle, float sunAngle);

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

    //-----------------------------------------------------------------------------------------------
    // [NEW] Phase color configuration accessors
    // These allow runtime modification of sky/fog colors via ImGui

    static SkyPhaseColors& GetSkyColors();
    static SkyPhaseColors& GetFogColors();

    static void SetSkyColors(const SkyPhaseColors& colors);
    static void SetFogColors(const SkyPhaseColors& colors);

    static void ResetSkyColorsToDefault();
    static void ResetFogColorsToDefault();

    //-----------------------------------------------------------------------------------------------
    /**
     * @brief Calculate sky color blended with fog based on elevation angle (CPU-side fog)
     *
     * This function implements Iris-style CPU fog blending. Instead of GPU fog calculations,
     * the sky color is pre-blended with fog color based on viewing elevation angle.
     * This ensures shader packs see the correct skyColor without needing custom fog code.
     *
     * @param celestialAngle Current celestial angle (0.0 - 1.0), from TimeOfDayManager
     * @param elevationAngle Elevation angle in degrees: 90° = zenith (pure sky), 0° = horizon (pure fog)
     * @return Vec3 RGB color blended between skyColor and fogColor
     *
     * @note Blend formula: lerp(fogColor, skyColor, elevationFactor)
     * @note elevationFactor = pow(sin(elevation), 0.5) for smooth falloff
     * @note At zenith (90°): returns pure skyColor
     * @note At horizon (0°): returns pure fogColor
     *
     * @note Reference: Iris FogMode.OFF for SKY_BASIC - CPU skyColor includes fog blending
     * @note Reference: Minecraft ClientLevel.getSkyColor() considers view direction
     */
    static Vec3 CalculateSkyColorWithFog(float celestialAngle, float elevationDegrees);

    /**
     * @brief Convert vertex position to elevation angle (relative to camera)
     *
     * Utility function to calculate the elevation angle from a vertex position.
     * Used when generating sky dome vertices with fog-blended colors.
     *
     * @param vertexPos Vertex position in local space (relative to camera at origin)
     * @return float Elevation angle in degrees (0° = horizon, 90° = zenith, -90° = nadir)
     *
     * @note Formula: elevation = atan2(z, sqrt(x*x + y*y)) * RAD_TO_DEG
     * @note For sky dome: center (0,0,16) = 90°, perimeter (x,y,0) = 0°
     */
    static float CalculateElevationAngle(const Vec3& vertexPos);

private:
    // [NEW] Static storage for configurable phase colors
    static SkyPhaseColors s_skyColors;
    static SkyPhaseColors s_fogColors;
    //-----------------------------------------------------------------------------------------------
    /**
     * @brief Calculate daylight factor from celestialAngle (matches Minecraft vanilla)
     * 
     * Computes a factor representing daylight intensity (1.0 = full day, 0.0 = full night).
     * Uses cosine function to create smooth day/night transitions.
     * 
     * @param celestialAngle Current celestial angle (0.0 - 1.0), directly from getTimeOfDay()
     * @return float Daylight factor (0.0 - 1.0, clamped)
     * 
     * @note [FIX] Use celestialAngle for day factor calculation
     * @note Formula: cos(celestialAngle * 2PI) * 2 + 0.5, clamped to [0, 1]
     * @note Same algorithm as TimeOfDayManager::CalculateCloudColor()
     */
    static float CalculateDayFactor(float celestialAngle);

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
