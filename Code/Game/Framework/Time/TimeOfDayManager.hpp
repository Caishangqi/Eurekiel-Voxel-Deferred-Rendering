#pragma once

// Forward declaration
class Clock;
struct Vec3;
struct Mat44;

/**
 * TimeOfDayManager - Minecraft style 24000 tick day and night cycle system
 *
 * Responsibilities:
 * 1. Time conversion: real time -> game tick (20 tick/second)
 * 2. Celestial body angle calculation: 0.0-1.0 cycle
 * 3. Compensation angle calculation: add 0.25 offset
 * 4. Cloud time calculation: 0.03 scaling factor
 *
 * Usage example:
 * TimeOfDayManager todManager(&gameClock);
 * todManager.Update();
 * float angle = todManager.GetCelestialAngle();
 */
class TimeOfDayManager
{
public:
    explicit TimeOfDayManager(Clock* gameClock);

    void Update();

    // Query methods
    int   GetCurrentTick() const;
    int   GetDayCount() const;
    float GetCelestialAngle() const;
    float GetCompensatedCelestialAngle() const;
    float GetCloudTime() const;

    // [FIX P1] Calculate celestial body direction vectors (moved from SkyRenderPass)
    // These methods follow Iris CelestialUniforms.java:119-133 for position calculation
    // Returns VIEW SPACE DIRECTION VECTOR (w=0), not position!
    // The vector points toward the celestial body, ignoring camera translation.
    // @param gbufferModelView The World->Camera transform matrix (from BeginCamera)
    Vec3 CalculateSunPosition(const Mat44& gbufferModelView) const;
    Vec3 CalculateMoonPosition(const Mat44& gbufferModelView) const;

private:
    // Internal helper: Calculate celestial direction vector in view space
    // @param y Initial direction magnitude (100 for sun, -100 for moon)
    // @param gbufferModelView The World->Camera transform matrix
    // @return View space direction vector (w=0), length ~100 units
    Vec3 CalculateCelestialPosition(float y, const Mat44& gbufferModelView) const;

    // Constants
    static constexpr float SECONDS_PER_TICK       = 0.05f; // 20 ticks per second
    static constexpr int   TICKS_PER_DAY          = 24000; // Full day-night cycle
    static constexpr float CLOUD_TIME_SCALE       = 0.03f; // Cloud animation speed
    static constexpr float CELESTIAL_ANGLE_OFFSET = 0.25f; // Sun/Moon phase offset

    // Member variables
    Clock* m_clock           = nullptr;
    float  m_accumulatedTime = 0.0f;
    int    m_currentTick     = 0;
    int    m_dayCount        = 0;

    /// Imgui Debugger
    bool showDemoWindow = true;
};
