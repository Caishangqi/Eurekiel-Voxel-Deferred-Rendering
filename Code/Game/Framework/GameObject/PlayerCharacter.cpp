#include "PlayerCharacter.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Gameplay/Game.hpp"
using namespace enigma::graphic;

PlayerCharacter::PlayerCharacter(Game* parent) : GameObject(parent)
{
    // [REFACTOR] Use PerspectiveCamera instead of deprecated EnigmaCamera
    // Parameters: position, orientation, fov, aspectRatio, nearPlane, farPlane
    constexpr float DEFAULT_FOV          = 90.0f;
    constexpr float DEFAULT_ASPECT_RATIO = 16.0f / 9.0f;
    constexpr float DEFAULT_NEAR_PLANE   = 0.1f;
    constexpr float DEFAULT_FAR_PLANE    = 1000.0f;

    m_camera = std::make_unique<PerspectiveCamera>(
        Vec3::ZERO, // Initial position
        EulerAngles(), // Initial orientation
        DEFAULT_FOV,
        DEFAULT_ASPECT_RATIO,
        DEFAULT_NEAR_PLANE,
        DEFAULT_FAR_PLANE
    );
}

PlayerCharacter::~PlayerCharacter()
{
}

void PlayerCharacter::Update(float deltaSeconds)
{
    GameObject::Update(deltaSeconds);
    HandleInputAction(deltaSeconds);
    UpdateCamera(deltaSeconds);
    UpdatePlayerStatus(deltaSeconds);
}

void PlayerCharacter::Render() const
{
    g_theRendererSubsystem->BeginCamera(*m_camera.get());
    g_theRendererSubsystem->EndCamera(*m_camera.get());
}

Mat44 PlayerCharacter::GetModelToWorldTransform() const
{
    return GameObject::GetModelToWorldTransform();
}

enigma::graphic::PerspectiveCamera* PlayerCharacter::GetCamera() const
{
    return m_camera.get();
}

void PlayerCharacter::HandleInputAction(float deltaSeconds)
{
    Vec2 cursorDelta             = g_theInput->GetCursorClientDelta();
    m_orientation.m_yawDegrees   += -cursorDelta.x * 0.125f;
    m_orientation.m_pitchDegrees += -cursorDelta.y * 0.125f;
    float speed                  = 2.0f;
    if (g_theInput->IsKeyDown(KEYCODE_LEFT_SHIFT))speed *= 10.f;
    if (g_theInput->IsKeyDown('Q'))m_orientation.m_rollDegrees += 0.125f;

    if (g_theInput->IsKeyDown('E'))m_orientation.m_rollDegrees -= 0.125f;

    m_orientation.m_pitchDegrees = GetClamped(m_orientation.m_pitchDegrees, -85.f, 85.f);
    m_orientation.m_rollDegrees  = GetClamped(m_orientation.m_rollDegrees, -45.f, 45.f);

    Vec3 forward, left, up;
    m_orientation.GetAsVectors_IFwd_JLeft_KUp(forward, left, up);

    if (g_theInput->IsKeyDown('W'))m_position += forward * speed * deltaSeconds;

    if (g_theInput->IsKeyDown('S'))m_position -= forward * speed * deltaSeconds;

    if (g_theInput->IsKeyDown('A')) m_position += left * speed * deltaSeconds;

    if (g_theInput->IsKeyDown('D'))m_position -= left * speed * deltaSeconds;

    if (g_theInput->IsKeyDown('Z'))m_position.z -= deltaSeconds * speed;

    if (g_theInput->IsKeyDown('C'))m_position.z += deltaSeconds * speed;
}

void PlayerCharacter::UpdateCamera(float deltaSeconds)
{
    UNUSED(deltaSeconds)
    m_camera->SetOrientation(m_orientation);
    m_camera->SetPosition(m_position);
}

void PlayerCharacter::UpdatePlayerStatus(float deltaSeconds)
{
    UNUSED(deltaSeconds)
    auto world = g_theGame->GetWorld();
    if (world)
    {
        auto blockState = world->GetBlockState(enigma::voxel::BlockPos((int32_t)m_position.x, (int32_t)m_position.y, (int32_t)m_position.z));
        if (!blockState->GetFluidState().IsEmpty())
        {
            // [REFACTOR] Use global COMMON_UNIFORM instead of m_playerStatusUniform
            COMMON_UNIFORM.isEyeInWater = 1;
        }
        else
        {
            COMMON_UNIFORM.isEyeInWater = 0;
        }
    }
    // [REFACTOR] COMMON_UNIFORM is uploaded in RenderPass, not here
    // The upload happens in the appropriate RenderPass that needs this data
}
