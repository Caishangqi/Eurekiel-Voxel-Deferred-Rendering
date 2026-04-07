#include "PlayerCameraRig.hpp"

#include "Engine/Graphic/Camera/PerspectiveCamera.hpp"
#include "Game/Framework/Camera/GameCameraDebugState.hpp"

using namespace enigma::graphic;

PlayerCameraRig::PlayerCameraRig(float fov, float aspectRatio, float nearPlane, float farPlane)
{
    m_gameplayCamera = std::make_unique<PerspectiveCamera>(
        Vec3::ZERO,
        EulerAngles(),
        fov,
        aspectRatio,
        nearPlane,
        farPlane);

    m_debugCamera = std::make_unique<PerspectiveCamera>(
        Vec3::ZERO,
        EulerAngles(),
        fov,
        aspectRatio,
        nearPlane,
        farPlane);

    m_isDetachedDebugCameraEnabled = GameCameraDebugState::IsDetachedDebugCameraEnabled();
    m_detachedDebugCameraModeChangedHandle = GameCameraDebugState::OnDetachedDebugCameraModeChanged.Add(
        this,
        &PlayerCameraRig::HandleDetachedDebugCameraModeChanged);
    m_debugCameraSyncRequestedHandle = GameCameraDebugState::OnDebugCameraSyncRequested.Add(
        this,
        &PlayerCameraRig::HandleDebugCameraSyncRequested);

    if (m_isDetachedDebugCameraEnabled)
    {
        m_hasPendingDebugCameraSyncRequest = true;
    }
}

PlayerCameraRig::~PlayerCameraRig()
{
    if (m_detachedDebugCameraModeChangedHandle != 0)
    {
        GameCameraDebugState::OnDetachedDebugCameraModeChanged.Remove(m_detachedDebugCameraModeChangedHandle);
        m_detachedDebugCameraModeChangedHandle = 0;
    }

    if (m_debugCameraSyncRequestedHandle != 0)
    {
        GameCameraDebugState::OnDebugCameraSyncRequested.Remove(m_debugCameraSyncRequestedHandle);
        m_debugCameraSyncRequestedHandle = 0;
    }
}

PerspectiveCamera* PlayerCameraRig::GetGameplayCamera() const
{
    return m_gameplayCamera.get();
}

PerspectiveCamera* PlayerCameraRig::GetRenderCamera() const
{
    if (m_isDetachedDebugCameraEnabled && m_debugCamera)
    {
        return m_debugCamera.get();
    }

    return m_gameplayCamera.get();
}

PerspectiveCamera* PlayerCameraRig::GetDebugCamera() const
{
    return m_debugCamera.get();
}

PerspectiveCamera* PlayerCameraRig::GetChunkBatchCullingCamera() const
{
    return m_gameplayCamera.get();
}

bool PlayerCameraRig::IsDetachedDebugCameraEnabled() const
{
    return m_isDetachedDebugCameraEnabled;
}

void PlayerCameraRig::UpdateGameplayCamera(const Vec3& position, const EulerAngles& orientation)
{
    if (!m_gameplayCamera)
    {
        return;
    }

    m_gameplayCamera->SetPositionAndOrientation(position, orientation);
    CopyGameplayProjectionToDebugCamera();

    if (m_hasPendingDebugCameraSyncRequest)
    {
        SyncDebugCameraToGameplayCamera();
    }
}

void PlayerCameraRig::UpdateDebugCamera(const Vec3& position, const EulerAngles& orientation)
{
    if (!m_debugCamera)
    {
        return;
    }

    m_debugCamera->SetPositionAndOrientation(position, orientation);
}

void PlayerCameraRig::SyncDebugCameraToGameplayCamera()
{
    if (!m_gameplayCamera || !m_debugCamera)
    {
        return;
    }

    m_debugCamera->SetPositionAndOrientation(
        m_gameplayCamera->GetPosition(),
        m_gameplayCamera->GetOrientation());
    CopyGameplayProjectionToDebugCamera();
    m_hasPendingDebugCameraSyncRequest = false;
}

void PlayerCameraRig::CopyGameplayProjectionToDebugCamera() const
{
    if (!m_gameplayCamera || !m_debugCamera)
    {
        return;
    }

    m_debugCamera->SetFOV(m_gameplayCamera->GetFOV());
    m_debugCamera->SetAspectRatio(m_gameplayCamera->GetAspectRatio());
    m_debugCamera->SetNearFar(m_gameplayCamera->GetNearPlane(), m_gameplayCamera->GetFarPlane());
}

void PlayerCameraRig::HandleDetachedDebugCameraModeChanged(bool enabled)
{
    m_isDetachedDebugCameraEnabled = enabled;
    if (enabled)
    {
        m_hasPendingDebugCameraSyncRequest = true;
    }
}

void PlayerCameraRig::HandleDebugCameraSyncRequested()
{
    m_hasPendingDebugCameraSyncRequest = true;
}
