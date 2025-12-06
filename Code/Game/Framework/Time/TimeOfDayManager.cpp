#include "Game/Framework/Time/TimeOfDayManager.hpp"

#include "Engine/Core/Clock.hpp"

// Helper function for fractional part calculation
static double Frac(double value)
{
    return value - floor(value);
}

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/ImGui/ImGuiSubsystem.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Game/GameCommon.hpp"
#include "ThirdParty/imgui/imgui.h"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/Mat44.hpp"
#include <cmath> // std::sin, std::cos

TimeOfDayManager::TimeOfDayManager(Clock* gameClock)
    : m_clock(gameClock)
      , m_accumulatedTime(0.0f)
      , m_currentTick(0)
      , m_dayCount(0)
{
    g_theImGui->RegisterWindow("ImGuiDemo", [this]()
    {
        ImGui::ShowDemoWindow(&showDemoWindow);
    });
}

void TimeOfDayManager::Update()
{
    if (!m_clock) return;

    float deltaSeconds = m_clock->GetDeltaSeconds();
    m_accumulatedTime  += deltaSeconds * m_timeScale;

    // [NEW] 累积总tick数用于云动画（连续递增，不循环）
    // 这样云动画就不会因为日夜循环而瞬移
    m_totalTicks += (deltaSeconds * m_timeScale) / SECONDS_PER_TICK;

    int deltaTicks = static_cast<int>(m_accumulatedTime / SECONDS_PER_TICK);

    if (deltaTicks > 0)
    {
        m_accumulatedTime -= deltaTicks * SECONDS_PER_TICK;
        m_currentTick     += deltaTicks;

        while (m_currentTick >= TICKS_PER_DAY)
        {
            m_currentTick -= TICKS_PER_DAY;
            m_dayCount++;
        }
    }

    if (g_theInput->WasKeyJustPressed('U'))
    {
        showDemoWindow = !showDemoWindow;
    }
}

int TimeOfDayManager::GetCurrentTick() const
{
    return m_currentTick;
}

int TimeOfDayManager::GetDayCount() const
{
    return m_dayCount;
}

// Reference: Minecraft DimensionType.java:113-116 timeOfDay()
// Non-linear time progression with -0.25 offset and cosine smoothing
// Formula: d = frac(tick/24000 - 0.25), e = 0.5 - cos(d*PI)/2, return (d*2 + e)/3
float TimeOfDayManager::GetCelestialAngle() const
{
    double d = Frac(static_cast<double>(m_currentTick) / 24000.0 - 0.25);
    double e = 0.5 - std::cos(d * PI) / 2.0;
    return static_cast<float>((d * 2.0 + e) / 3.0);
}

float TimeOfDayManager::GetCompensatedCelestialAngle() const
{
    float angle = GetCelestialAngle() + CELESTIAL_ANGLE_OFFSET;
    if (angle >= 1.0f)
    {
        angle -= 1.0f;
    }
    return angle;
}

float TimeOfDayManager::GetSunAngle() const
{
    // [FIX] Reference: Iris CelestialUniforms.java:24-32
    // sunAngle is used for sunrise/sunset calculations
    // It's offset by +0.25 from celestialAngle so that:
    // - sunAngle=0.0 corresponds to sunrise (celestialAngle=0.75 or tick=18000)
    // - sunAngle=0.5 corresponds to sunset (celestialAngle=0.25 or tick=6000)
    float skyAngle = GetCelestialAngle();

    if (skyAngle < 0.75f)
    {
        return skyAngle + 0.25f;
    }
    else
    {
        return skyAngle - 0.75f;
    }
}

float TimeOfDayManager::GetCloudTime() const
{
    // [FIX] 使用连续累积的总tick数（m_totalTicks）而非日内循环tick（m_currentTick）
    // 参考：Sodium CloudRenderer.java Line 77
    //   double cloudTime = (double)((ticks + tickDelta) * 0.03F);
    // 其中 ticks 是游戏总tick数，不是日内循环tick
    //
    // 这样云就会平滑连续移动，不会因为日夜循环（tick从23999回到0）而瞬移
    return static_cast<float>(m_totalTicks * CLOUD_TIME_SCALE);
}

// =============================================================================
// [FIX] Sun/Moon Direction Vector Calculation - VIEW SPACE
// =============================================================================
// Reference: Iris CelestialUniforms.java:119-133 getCelestialPosition()
//
// Key Implementation: Returns VIEW SPACE DIRECTION VECTOR (w=0), not position.
// Uses TransformVectorQuantity3D to ignore camera translation, ensuring
// celestial bodies maintain correct orientation regardless of player movement.
//
// Algorithm:
// 1. Start with initial direction (0, 0, y) where y=100 for sun, y=-100 for moon
// 2. Apply Y-axis rotation (celestialAngle * 360 degrees) for day/night cycle
// 3. Transform as direction vector (w=0) through gbufferModelView
// 4. Result: View space direction pointing toward celestial body
// =============================================================================

Vec3 TimeOfDayManager::CalculateCelestialPosition(float y, const Mat44& gbufferModelView) const
{
    // ==========================================================================
    // Coordinate System Adaptation: Iris (Y-up) -> Engine (Z-up)
    // ==========================================================================
    // Iris/Minecraft: Y-up, Z-forward (OpenGL style)
    // Our Engine: Z-up, X-forward
    //
    // Iris sun path: rotates around X-axis (east-west), Y is up
    // Our sun path: rotates around Y-axis (left-right), Z is up
    //
    // Mapping: Iris Y -> Engine Z, Iris X -> Engine Y, Iris Z -> Engine X
    // ==========================================================================

    // [Step 1] Initial direction - start pointing UP (+Z in our engine)
    // In Iris this would be (0, y, 0) pointing up along Y
    // In our engine, up is Z, so we use (0, 0, y)
    Vec3 direction(0.0f, 0.0f, y);

    // [Step 2] Get sky angle for rotation
    // [FIX] Use celestialAngle directly (no additional offset needed)
    // Reference: Iris CelestialUniforms.java:128
    //   celestial.rotate(Axis.XP.rotationDegrees(getSkyAngle() * 360.0F));
    // where getSkyAngle() = timeOfDay() which already includes -0.25 offset
    //
    // Minecraft time mapping (after timeOfDay() formula):
    // - tick=0     (sunrise)  -> celestialAngle≈0.00 -> angle=0°   (horizon, east)
    // - tick=6000  (noon)     -> celestialAngle≈0.25 -> angle=90°  (zenith)
    // - tick=12000 (sunset)   -> celestialAngle≈0.50 -> angle=180° (horizon, west)
    // - tick=13000 (night begins) -> celestialAngle≈0.54 -> angle=195° (below horizon, Minecraft official: TimeCommand.java:22)
    // - tick=18000 (midnight) -> celestialAngle≈0.75 -> angle=270° (nadir)
    float celestialAngle = GetCelestialAngle();
    float angleDegrees   = celestialAngle * 360.0f; // Direct conversion, no offset

    // [Step 3] Build transformation matrix
    // In our Z-up coordinate system:
    // - Sun rises in East (-Y), peaks at Zenith (+Z), sets in West (+Y)
    // - This requires rotation around Y-axis (left-right axis)
    Mat44 celestial = gbufferModelView;

    // Apply Y rotation for celestial angle (sun path rotation)
    // This rotates the sun from +Z (up) around the Y-axis
    Mat44 rotY = Mat44::MakeYRotationDegrees(angleDegrees);
    celestial.Append(rotY);

    // [Step 4] Transform as DIRECTION VECTOR (w=0), not position!
    // This ignores the translation component of gbufferModelView
    // Result is a VIEW SPACE direction pointing toward the celestial body
    direction = celestial.TransformVectorQuantity3D(direction);

    return direction;
}

Vec3 TimeOfDayManager::CalculateSunPosition(const Mat44& gbufferModelView) const
{
    // Sun direction: initial magnitude +100 (pointing upward in local space)
    // Transformed to view space direction vector (w=0)
    return CalculateCelestialPosition(100.0f, gbufferModelView);
}

Vec3 TimeOfDayManager::CalculateMoonPosition(const Mat44& gbufferModelView) const
{
    // Moon direction: initial magnitude -100 (opposite to sun in local space)
    // Transformed to view space direction vector (w=0)
    return CalculateCelestialPosition(-100.0f, gbufferModelView);
}

void TimeOfDayManager::SetTimeScale(float timeScale)
{
    m_timeScale = timeScale;
}

float TimeOfDayManager::GetTimeScale() const
{
    return m_timeScale;
}

void TimeOfDayManager::SetCurrentTick(int tick)
{
    // Wrap tick value to valid range [0, 23999]
    m_currentTick = tick % TICKS_PER_DAY;
    if (m_currentTick < 0)
    {
        m_currentTick += TICKS_PER_DAY;
    }
    // Reset accumulated time to prevent immediate tick advancement
    m_accumulatedTime = 0.0f;
}

// =============================================================================
// [NEW] Cloud Color Calculation - Minecraft Algorithm
// =============================================================================
// Reference: Minecraft ClientLevel.java:673-704 getCloudColor()
//
// Algorithm:
// 1. Get celestialAngle (0.0 - 1.0, where 0.25 = noon, 0.75 = midnight)
// 2. Calculate daylight factor: h = cos(celestialAngle * 2PI) * 2 + 0.5, clamped to [0,1]
// 3. Base color is white (1, 1, 1)
// 4. Apply rain effect: mix toward grayscale
// 5. Apply daylight factor:
//    - R *= h * 0.9 + 0.1  (darkens at night, min 0.1)
//    - G *= h * 0.9 + 0.1
//    - B *= h * 0.85 + 0.15  (blue slightly brighter at night)
// 6. Apply thunder effect: further darkening
// =============================================================================

Vec3 TimeOfDayManager::CalculateCloudColor(float rainLevel, float thunderLevel) const
{
    // [Step 1] Get celestial angle (0.0 - 1.0)
    float celestialAngle = GetCelestialAngle();

    // [Step 2] Calculate daylight factor
    // Reference: ClientLevel.java Line 681
    // float h = Mth.cos(g * ((float)Math.PI * 2)) * 2.0F + 0.5F;
    // h = Mth.clamp(h, 0.0F, 1.0F);
    constexpr float TWO_PI = 6.2831853f;
    float           h      = std::cos(celestialAngle * TWO_PI) * 2.0f + 0.5f;
    h                      = (h < 0.0f) ? 0.0f : ((h > 1.0f) ? 1.0f : h); // clamp(0, 1)

    // [Step 3] Base color (white)
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;

    // [Step 4] Apply rain effect (mix toward grayscale)
    // Reference: ClientLevel.java Line 686-691
    if (rainLevel > 0.0f)
    {
        // Calculate grayscale value
        float grayscale = (r * 0.3f + g * 0.59f + b * 0.11f) * 0.6f;
        // Mix toward grayscale based on rain intensity
        float rainFactor = rainLevel * 0.95f;
        r                = r * (1.0f - rainFactor) + grayscale * rainFactor;
        g                = g * (1.0f - rainFactor) + grayscale * rainFactor;
        b                = b * (1.0f - rainFactor) + grayscale * rainFactor;
    }

    // [Step 5] Apply daylight factor
    // Reference: ClientLevel.java Line 693-695
    // Note: Blue channel has slightly different coefficient for atmospheric effect
    r *= h * 0.9f + 0.1f; // Range: [0.1, 1.0]
    g *= h * 0.9f + 0.1f; // Range: [0.1, 1.0]
    b *= h * 0.85f + 0.15f; // Range: [0.15, 1.0] - blue slightly brighter at night

    // [Step 6] Apply thunder effect (further darkening)
    // Reference: ClientLevel.java Line 697-702
    if (thunderLevel > 0.0f)
    {
        // Calculate dimmed value
        float dimFactor = (r * 0.3f + g * 0.59f + b * 0.11f) * 0.2f;
        // Mix toward darker based on thunder intensity
        float thunderFactor = thunderLevel * 0.75f;
        r                   = r * (1.0f - thunderFactor) + dimFactor * thunderFactor;
        g                   = g * (1.0f - thunderFactor) + dimFactor * thunderFactor;
        b                   = b * (1.0f - thunderFactor) + dimFactor * thunderFactor;
    }

    return Vec3(r, g, b);
}
