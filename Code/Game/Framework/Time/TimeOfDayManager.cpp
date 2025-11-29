#include "Game/Framework/Time/TimeOfDayManager.hpp"

#include "Engine/Core/Clock.hpp"
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
    m_accumulatedTime  += deltaSeconds;

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

float TimeOfDayManager::GetCelestialAngle() const
{
    return static_cast<float>(m_currentTick) / static_cast<float>(TICKS_PER_DAY);
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

float TimeOfDayManager::GetCloudTime() const
{
    float tickDelta = m_accumulatedTime / SECONDS_PER_TICK;
    return (static_cast<float>(m_currentTick) + tickDelta) * CLOUD_TIME_SCALE;
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
    float celestialAngle = GetCelestialAngle();
    float angleDegrees   = celestialAngle * 360.0f;

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
