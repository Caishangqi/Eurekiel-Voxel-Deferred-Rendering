#pragma once
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/Vec4.hpp"

//-----------------------------------------------------------------------------------------------
/**
 * @brief Bezier easing control points for a single phase transition
 *
 * Uses cubic Bezier curve for time remapping (like CSS cubic-bezier).
 * The curve maps linear t [0,1] to eased t [0,1].
 *
 * Control points:
 * - P0 = (0, 0) - Start point (implicit)
 * - P1 = (x1, y1) - First control point
 * - P2 = (x2, y2) - Second control point
 * - P3 = (1, 1) - End point (implicit)
 *
 * Common presets:
 * - Linear:      (0.0, 0.0), (1.0, 1.0) - No easing
 * - EaseIn:      (0.42, 0.0), (1.0, 1.0) - Slow start
 * - EaseOut:     (0.0, 0.0), (0.58, 1.0) - Slow end
 * - EaseInOut:   (0.42, 0.0), (0.58, 1.0) - Slow start and end
 * - HoldStart:   (0.8, 0.0), (0.9, 0.1) - Stay at start longer
 * - HoldEnd:     (0.1, 0.9), (0.2, 1.0) - Stay at end longer
 */
struct BezierEasing
{
    Vec2 p1; // First control point (x1, y1)
    Vec2 p2; // Second control point (x2, y2)

    // Default: linear (no easing)
    BezierEasing() : p1(0.0f, 0.0f), p2(1.0f, 1.0f)
    {
    }

    BezierEasing(float x1, float y1, float x2, float y2) : p1(x1, y1), p2(x2, y2)
    {
    }

    BezierEasing(const Vec2& cp1, const Vec2& cp2) : p1(cp1), p2(cp2)
    {
    }

    // Common presets
    static BezierEasing Linear() { return BezierEasing(0.0f, 0.0f, 1.0f, 1.0f); }
    static BezierEasing EaseIn() { return BezierEasing(0.42f, 0.0f, 1.0f, 1.0f); }
    static BezierEasing EaseOut() { return BezierEasing(0.0f, 0.0f, 0.58f, 1.0f); }
    static BezierEasing EaseInOut() { return BezierEasing(0.42f, 0.0f, 0.58f, 1.0f); }

    // [IMPORTANT] Hold presets for extending phases like midnight
    static BezierEasing HoldStart() { return BezierEasing(0.8f, 0.0f, 0.9f, 0.1f); } // Stay at start color longer
    static BezierEasing HoldEnd() { return BezierEasing(0.1f, 0.9f, 0.2f, 1.0f); } // Stay at end color longer
    static BezierEasing HoldMiddle() { return BezierEasing(0.3f, 0.5f, 0.7f, 0.5f); } // Pause in middle

    /**
     * @brief Apply Bezier easing to a linear t value
     *
     * Uses cubic Bezier evaluation for time remapping.
     *
     * @param t Linear parameter [0, 1]
     * @return Eased parameter [0, 1]
     */
    float Apply(float t) const;
};

//-----------------------------------------------------------------------------------------------
/**
 * @brief Easing configuration for all 5 phase transitions
 *
 * Each transition between phases can have its own Bezier easing curve.
 * This allows fine-tuned control over how long each phase "holds" and
 * how quickly transitions occur.
 *
 * Phase transitions:
 * - noonToSunset:       Phase 0 (0.0 - 0.25)
 * - sunsetToMidnight:   Phase 1 (0.25 - 0.5)
 * - midnightToSunrise:  Phase 2 (0.5 - 0.75)
 * - sunriseToDawn:      Phase 3 (0.75 - 0.79)
 * - dawnToNoon:         Phase 4 (0.79 - 1.0)
 */
struct SkyEasingConfig
{
    BezierEasing noonToSunset; // Phase 0: Day to evening
    BezierEasing sunsetToMidnight; // Phase 1: Evening to night (make this slow = longer night)
    BezierEasing midnightToSunrise; // Phase 2: Night to morning (make this slow = longer night)
    BezierEasing sunriseToDawn; // Phase 3: Sunrise glow to dawn
    BezierEasing dawnToNoon; // Phase 4: Morning to midday

    // Default: midnightToSunrise uses HoldStart for longer night
    static SkyEasingConfig GetDefault()
    {
        SkyEasingConfig config;
        config.noonToSunset      = BezierEasing::EaseInOut(); // Smooth day to evening
        config.sunsetToMidnight  = BezierEasing(0.1f, 0.8f, 0.2f, 1.0f); // Quick to midnight, hold
        config.midnightToSunrise = BezierEasing(0.8f, 0.0f, 0.9f, 0.2f); // Hold midnight, then quick sunrise
        config.sunriseToDawn     = BezierEasing::EaseOut(); // Quick sunrise transition
        config.dawnToNoon        = BezierEasing::EaseIn(); // Gradual morning to noon
        return config;
    }

    // Minecraft-like: longer night, quick sunrise/sunset transitions
    static SkyEasingConfig GetMinecraftStyle()
    {
        SkyEasingConfig config;
        config.noonToSunset      = BezierEasing::EaseInOut(); // Smooth day to evening
        config.sunsetToMidnight  = BezierEasing(0.1f, 0.8f, 0.2f, 1.0f); // Quick to midnight, hold
        config.midnightToSunrise = BezierEasing(0.8f, 0.0f, 0.9f, 0.2f); // Hold midnight, then quick sunrise
        config.sunriseToDawn     = BezierEasing::EaseOut(); // Quick sunrise transition
        config.dawnToNoon        = BezierEasing::EaseIn(); // Gradual morning to noon
        return config;
    }
};

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
    Vec3 sunset; // tick 12000 (6:00 PM) - Orange-red sunset
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
 * @brief Configuration structure for sunrise/sunset strip glow colors
 *
 * The sunrise strip is the horizontal glow band that appears at sunrise/sunset.
 * This is SEPARATE from the sky dome colors - the strip overlays the sky.
 *
 * @note In Minecraft, the strip uses getSunriseColor() which returns orange-red.
 *       We allow separate configuration for sunrise vs sunset strip colors.
 *
 * @note Reference: Minecraft DimensionSpecialEffects.java getSunriseColor()
 */
struct SunriseStripColors
{
    Vec3 sunriseStrip; // Strip color at sunrise (tick 23000 -> tick 1000)
    Vec3 sunsetStrip; // Strip color at sunset (tick 11000 -> tick 13000)

    // Default strip glow colors (Minecraft-inspired)
    static SunriseStripColors GetDefault()
    {
        SunriseStripColors colors;
        colors.sunriseStrip = Vec3(0.75f, 0.69f, 0.65f); // Warm orange-yellow sunrise
        colors.sunsetStrip  = Vec3(0.73f, 0.31f, 0.24f); // Orange-red sunset
        return colors;
    }
};

//-----------------------------------------------------------------------------------------------
/**
 * @brief Static helper class for calculating sky colors (5-phase interpolation with Bezier easing)
 *
 * This class provides utility functions to calculate sky, fog, and sunrise colors
 * using a 5-phase interpolation system with configurable Bezier easing curves.
 *
 * All methods are static and the class cannot be instantiated.
 *
 * @note Phase Mapping (based on celestialAngle from ITimeProvider::GetCelestialAngle()):
 *       - celestialAngle 0.0   = tick 6000  = 12:00 PM (Noon)
 *       - celestialAngle 0.25  = tick 12000 = 6:00 PM  (Sunset)
 *       - celestialAngle 0.5   = tick 18000 = 12:00 AM (Midnight)
 *       - celestialAngle 0.75  = tick 0     = 6:00 AM  (Sunrise)
 *       - celestialAngle 0.79  = tick 1000  = 7:00 AM  (Dawn)
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
     * @brief Calculate sky color using 5-phase interpolation with Bezier easing
     *
     * Computes the sky dome color by interpolating between 5 key colors based on celestialAngle.
     * Each phase transition uses a configurable Bezier easing curve for non-linear timing.
     *
     * @param celestialAngle Current celestial angle (0.0 - 1.0), from ITimeProvider
     * @return Vec3 RGB color values (0.0 - 1.0 range)
     */
    static Vec3 CalculateSkyColor(float celestialAngle);

    /**
     * @brief Calculate fog color using 5-phase interpolation with Bezier easing
     *
     * Computes the fog color used for clearing the render target (Clear RT).
     * Fog colors are lighter/more desaturated than sky colors to create atmospheric depth.
     *
     * @param celestialAngle Current celestial angle (0.0 - 1.0)
     * @param sunAngle Kept for API compatibility (not used in simple mode)
     * @return Vec3 RGB fog color (0.0 - 1.0 range)
     */
    static Vec3 CalculateFogColor(float celestialAngle, float sunAngle);

    /**
     * @brief Calculate sunrise/sunset glow color based on celestialAngle (timeOfDay)
     *
     * Computes the orange glow color and alpha for sunrise/sunset rendering.
     * Uses Minecraft's exact formula: cos(celestialAngle * 2π) in [-0.4, 0.4] window.
     *
     * @param celestialAngle timeOfDay value (0.0-1.0): 0.0=sunrise, 0.25=noon, 0.5=sunset, 0.75=midnight
     * @return Vec4 RGBA color (RGB: orange tint, A: intensity 0.0-1.0)
     *
     * @note Reference: Minecraft DimensionSpecialEffects.java:44-60 getSunriseColor()
     */
    static Vec4 CalculateSunriseColor(float celestialAngle);

    //-----------------------------------------------------------------------------------------------
    // Phase color configuration accessors

    static SkyPhaseColors& GetSkyColors();
    static SkyPhaseColors& GetFogColors();

    static void SetSkyColors(const SkyPhaseColors& colors);
    static void SetFogColors(const SkyPhaseColors& colors);

    static void ResetSkyColorsToDefault();
    static void ResetFogColorsToDefault();

    // Sunrise strip color configuration accessors
    static SunriseStripColors& GetStripColors();
    static void                SetStripColors(const SunriseStripColors& colors);
    static void                ResetStripColorsToDefault();

    // [NEW] Easing configuration accessors
    static SkyEasingConfig& GetEasingConfig();
    static void             SetEasingConfig(const SkyEasingConfig& config);
    static void             ResetEasingToDefault();
    static void             SetMinecraftStyleEasing();

    //-----------------------------------------------------------------------------------------------
    /**
     * @brief Calculate sky color blended with fog based on elevation angle (CPU-side fog)
     *
     * This function implements Iris-style CPU fog blending.
     *
     * @param celestialAngle Current celestial angle (0.0 - 1.0), from ITimeProvider
     * @param elevationAngle Elevation angle in degrees: 90 = zenith, 0 = horizon
     * @return Vec3 RGB color blended between skyColor and fogColor
     */
    static Vec3 CalculateSkyColorWithFog(float celestialAngle, float elevationDegrees);

    /**
     * @brief Convert vertex position to elevation angle (relative to camera)
     *
     * @param vertexPos Vertex position in local space (relative to camera at origin)
     * @return float Elevation angle in degrees (0 = horizon, 90 = zenith, -90 = nadir)
     */
    static float CalculateElevationAngle(const Vec3& vertexPos);

private:
    // Static storage for configurable phase colors and easing
    static SkyPhaseColors     s_skyColors;
    static SkyPhaseColors     s_fogColors;
    static SunriseStripColors s_stripColors;
    static SkyEasingConfig    s_easingConfig;

    //-----------------------------------------------------------------------------------------------
    /**
     * @brief Calculate daylight factor from celestialAngle (matches Minecraft vanilla)
     */
    static float CalculateDayFactor(float celestialAngle);

    /**
     * @brief Calculate sunset/sunrise intensity factor
     * @param celestialAngle timeOfDay value (0.0-1.0), NOT Iris sunAngle
     * @note Reference: Minecraft DimensionSpecialEffects.java:44-59 getSunriseColor()
     */
    static float CalculateSunsetFactor(float celestialAngle);
};
