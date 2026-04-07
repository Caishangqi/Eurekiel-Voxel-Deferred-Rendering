#include "PlayerCharacter.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Graphic/Camera/PerspectiveCamera.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Framework/App.hpp"
#include "Game/Gameplay/Game.hpp"
using namespace enigma::graphic;

PlayerCharacter::PlayerCharacter(Game* parent) : GameObject(parent)
{
    // [REFACTOR] Use PerspectiveCamera instead of deprecated EnigmaCamera
    // Parameters: position, orientation, fov, aspectRatio, nearPlane, farPlane
    constexpr float DEFAULT_FOV          = 90.0f;
    constexpr float DEFAULT_ASPECT_RATIO = 16.0f / 9.0f;
    constexpr float DEFAULT_NEAR_PLANE   = 0.01f;
    float           DEFAULT_FAR_PLANE    = (float)(settings.GetInt("video.simulationDistance", 9) + 2) * enigma::voxel::Chunk::CHUNK_SIZE_X;

    m_cameraRig = std::make_unique<PlayerCameraRig>(
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
    if (g_theGame->m_enableSceneTest)
        return;
    UpdatePlayerStatus(deltaSeconds);
}

void PlayerCharacter::Render() const
{
    if (auto* renderCamera = GetRenderCamera())
    {
        g_theRendererSubsystem->BeginCamera(*renderCamera);
        g_theRendererSubsystem->EndCamera(*renderCamera);
    }
}

Mat44 PlayerCharacter::GetModelToWorldTransform() const
{
    return GameObject::GetModelToWorldTransform();
}

enigma::graphic::PerspectiveCamera* PlayerCharacter::GetCamera() const
{
    return m_cameraRig ? m_cameraRig->GetGameplayCamera() : nullptr;
}

enigma::graphic::PerspectiveCamera* PlayerCharacter::GetRenderCamera() const
{
    return m_cameraRig ? m_cameraRig->GetRenderCamera() : nullptr;
}

enigma::graphic::PerspectiveCamera* PlayerCharacter::GetDebugCamera() const
{
    return m_cameraRig ? m_cameraRig->GetDebugCamera() : nullptr;
}

void PlayerCharacter::SyncDebugCameraToGameplayCamera()
{
    if (!m_cameraRig)
    {
        return;
    }

    m_cameraRig->SyncDebugCameraToGameplayCamera();
}

void PlayerCharacter::HandleInputAction(float deltaSeconds)
{
    if (m_cameraRig && m_cameraRig->IsDetachedDebugCameraEnabled() && GetDebugCamera())
    {
        Vec3        debugPosition    = GetDebugCamera()->GetPosition();
        EulerAngles debugOrientation = GetDebugCamera()->GetOrientation();
        ApplyFreeCameraInput(debugPosition, debugOrientation, deltaSeconds);
        m_cameraRig->UpdateDebugCamera(debugPosition, debugOrientation);
        return;
    }

    ApplyFreeCameraInput(m_position, m_orientation, deltaSeconds);
}

void PlayerCharacter::UpdateCamera(float deltaSeconds)
{
    UNUSED(deltaSeconds)
    if (m_cameraRig)
    {
        m_cameraRig->UpdateGameplayCamera(m_position, m_orientation);
    }
}

void PlayerCharacter::UpdatePlayerStatus(float deltaSeconds)
{
    UNUSED(deltaSeconds)
    auto world = g_theGame->GetWorld();
    if (world)
    {
        enigma::voxel::BlockState* blockState = world->GetBlockState(enigma::voxel::BlockPos((int32_t)m_position.x, (int32_t)m_position.y, (int32_t)m_position.z));
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

void PlayerCharacter::ApplyFreeCameraInput(Vec3& position, EulerAngles& orientation, float deltaSeconds) const
{
    Vec2 cursorDelta               = g_theInput->GetCursorClientDelta();
    orientation.m_yawDegrees   += -cursorDelta.x * 0.125f;
    orientation.m_pitchDegrees += -cursorDelta.y * 0.125f;
    float speed                    = 2.0f;
    if (g_theInput->IsKeyDown(KEYCODE_LEFT_SHIFT))speed *= 10.f;
    if (g_theInput->IsKeyDown('Q'))orientation.m_rollDegrees += 0.125f;

    if (g_theInput->IsKeyDown('E'))orientation.m_rollDegrees -= 0.125f;

    orientation.m_pitchDegrees = GetClamped(orientation.m_pitchDegrees, -85.f, 85.f);
    orientation.m_rollDegrees  = GetClamped(orientation.m_rollDegrees, -45.f, 45.f);

    Vec3 forward, left, up;
    orientation.GetAsVectors_IFwd_JLeft_KUp(forward, left, up);

    if (g_theInput->IsKeyDown('W'))position += forward * speed * deltaSeconds;

    if (g_theInput->IsKeyDown('S'))position -= forward * speed * deltaSeconds;

    if (g_theInput->IsKeyDown('A')) position += left * speed * deltaSeconds;

    if (g_theInput->IsKeyDown('D'))position -= left * speed * deltaSeconds;

    if (g_theInput->IsKeyDown('Z'))position.z -= deltaSeconds * speed;

    if (g_theInput->IsKeyDown('C'))position.z += deltaSeconds * speed;
}
