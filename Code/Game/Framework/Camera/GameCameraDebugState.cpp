#include "GameCameraDebugState.hpp"

bool GameCameraDebugState::s_isDetachedDebugCameraEnabled = false;
bool GameCameraDebugState::s_drawPlayerCameraFrustum      = true;

enigma::event::MulticastDelegate<bool> GameCameraDebugState::OnDetachedDebugCameraModeChanged;
enigma::event::MulticastDelegate<>     GameCameraDebugState::OnDebugCameraSyncRequested;

bool GameCameraDebugState::IsDetachedDebugCameraEnabled()
{
    return s_isDetachedDebugCameraEnabled;
}

void GameCameraDebugState::SetDetachedDebugCameraEnabled(bool enabled)
{
    if (s_isDetachedDebugCameraEnabled == enabled)
    {
        return;
    }

    s_isDetachedDebugCameraEnabled = enabled;
    OnDetachedDebugCameraModeChanged.Broadcast(enabled);
}

bool GameCameraDebugState::ShouldDrawPlayerCameraFrustum()
{
    return s_drawPlayerCameraFrustum;
}

void GameCameraDebugState::SetDrawPlayerCameraFrustum(bool enabled)
{
    s_drawPlayerCameraFrustum = enabled;
}

void GameCameraDebugState::RequestDebugCameraSync()
{
    OnDebugCameraSyncRequested.Broadcast();
}
