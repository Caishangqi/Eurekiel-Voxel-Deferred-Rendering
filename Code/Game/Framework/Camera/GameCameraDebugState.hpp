#pragma once

#include "Engine/Core/Event/MulticastDelegate.hpp"

class GameCameraDebugState
{
public:
    GameCameraDebugState()                                         = delete;
    GameCameraDebugState(const GameCameraDebugState&)              = delete;
    GameCameraDebugState& operator=(const GameCameraDebugState&)   = delete;

    static bool IsDetachedDebugCameraEnabled();
    static void SetDetachedDebugCameraEnabled(bool enabled);

    static bool ShouldDrawPlayerCameraFrustum();
    static void SetDrawPlayerCameraFrustum(bool enabled);

    static void RequestDebugCameraSync();

public:
    static enigma::event::MulticastDelegate<bool> OnDetachedDebugCameraModeChanged;
    static enigma::event::MulticastDelegate<>     OnDebugCameraSyncRequested;

private:
    static bool s_isDetachedDebugCameraEnabled;
    static bool s_drawPlayerCameraFrustum;
};
