#include "Game/Framework/Time/TimeOfDayManager.hpp"

#include "Engine/Core/Clock.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/ImGui/ImGuiSubsystem.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Game/GameCommon.hpp"
#include "ThirdParty/imgui/imgui.h"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/Mat44.hpp"

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
// [FIX P1] Sun/Moon Position Calculation (moved from SkyRenderPass)
// =============================================================================
// Reference: Iris CelestialUniforms.java:108-122
// 
// Iris Implementation:
//   celestial.rotate(Axis.YP.rotationDegrees(-90.0F));  // Y-axis rotation
//   celestial.rotate(Axis.ZP.rotationDegrees(sunPathRotation));  // Z-axis rotation (sunPathRotation = 0 for now)
//   celestial.rotate(Axis.XP.rotationDegrees(getSkyAngle() * 360.0F));  // X-axis rotation
//   position = celestial.transform(position);
//
// Matrix Transformation Order (right-to-left):
//   1. Start with position (0, y, 0) where y = 100 for sun, y = -100 for moon
//   2. Apply X-axis rotation (celestial angle * 360 degrees)
//   3. Apply Z-axis rotation (sunPathRotation, currently 0)
//   4. Apply Y-axis rotation (-90 degrees)
// =============================================================================

Vec3 TimeOfDayManager::CalculateSunPosition() const
{
    // [Step 1] Initial position: (0, 100, 0) - Sun at +Y axis
    Vec3 position(0.0f, 100.0f, 0.0f);

    // [Step 2] Get celestial angle (0.0-1.0) and convert to degrees
    float celestialAngle = GetCelestialAngle();
    float angleDegrees   = celestialAngle * 360.0f;

    // [Step 3] Apply rotations (Iris order: X -> Z -> Y)
    // X-axis rotation (celestial angle)
    Mat44 rotX = Mat44::MakeXRotationDegrees(angleDegrees);
    position   = rotX.TransformPosition3D(position);

    // Z-axis rotation (sunPathRotation = 0, skip for now)
    // Mat44 rotZ = Mat44::CreateZRotationDegrees(0.0f);
    // position = rotZ.TransformPosition3D(position);

    // Y-axis rotation (-90 degrees)
    Mat44 rotY = Mat44::MakeYRotationDegrees(-90.0f);
    position   = rotY.TransformPosition3D(position);

    return position;
}

Vec3 TimeOfDayManager::CalculateMoonPosition() const
{
    // [Step 1] Initial position: (0, -100, 0) - Moon at -Y axis (opposite of sun)
    Vec3 position(0.0f, -100.0f, 0.0f);

    // [Step 2] Get celestial angle (0.0-1.0) and convert to degrees
    float celestialAngle = GetCelestialAngle();
    float angleDegrees   = celestialAngle * 360.0f;

    // [Step 3] Apply rotations (Iris order: X -> Z -> Y)
    // X-axis rotation (celestial angle)
    Mat44 rotX = Mat44::MakeXRotationDegrees(angleDegrees);
    position   = rotX.TransformPosition3D(position);

    // Z-axis rotation (sunPathRotation = 0, skip for now)
    // Mat44 rotZ = Mat44::CreateZRotationDegrees(0.0f);
    // position = rotZ.TransformPosition3D(position);

    // Y-axis rotation (-90 degrees)
    Mat44 rotY = Mat44::MakeYRotationDegrees(-90.0f);
    position   = rotY.TransformPosition3D(position);

    return position;
}
