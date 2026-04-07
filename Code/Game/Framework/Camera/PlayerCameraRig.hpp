#pragma once

#include <memory>

#include "Engine/Core/Event/MulticastDelegate.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include "Engine/Math/Vec3.hpp"

namespace enigma::graphic
{
    class PerspectiveCamera;
}

class PlayerCameraRig
{
public:
    PlayerCameraRig(float fov, float aspectRatio, float nearPlane, float farPlane);
    ~PlayerCameraRig();

    PlayerCameraRig(const PlayerCameraRig&)            = delete;
    PlayerCameraRig& operator=(const PlayerCameraRig&) = delete;

    enigma::graphic::PerspectiveCamera* GetGameplayCamera() const;
    enigma::graphic::PerspectiveCamera* GetRenderCamera() const;
    enigma::graphic::PerspectiveCamera* GetDebugCamera() const;
    enigma::graphic::PerspectiveCamera* GetChunkBatchCullingCamera() const;

    bool IsDetachedDebugCameraEnabled() const;

    void UpdateGameplayCamera(const Vec3& position, const EulerAngles& orientation);
    void UpdateDebugCamera(const Vec3& position, const EulerAngles& orientation);
    void SyncDebugCameraToGameplayCamera();

private:
    void CopyGameplayProjectionToDebugCamera() const;
    void HandleDetachedDebugCameraModeChanged(bool enabled);
    void HandleDebugCameraSyncRequested();

private:
    std::unique_ptr<enigma::graphic::PerspectiveCamera> m_gameplayCamera = nullptr;
    std::unique_ptr<enigma::graphic::PerspectiveCamera> m_debugCamera    = nullptr;
    bool                                                m_isDetachedDebugCameraEnabled = false;
    bool                                                m_hasPendingDebugCameraSyncRequest = false;
    enigma::event::DelegateHandle                       m_detachedDebugCameraModeChangedHandle = 0;
    enigma::event::DelegateHandle                       m_debugCameraSyncRequestedHandle       = 0;
};
