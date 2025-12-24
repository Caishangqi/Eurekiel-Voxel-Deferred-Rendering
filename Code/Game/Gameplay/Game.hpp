#pragma once
#include <memory>

#include "Engine/Core/Clock.hpp"
#include "Game/Framework/GameObject/PlayerCharacter.hpp"
#include "Game/Framework/RenderPass/SceneRenderPass.hpp"
#include "Game/Framework/Time/TimeOfDayManager.hpp"
#include "Game/SceneTest/SceneUnitTest.hpp"

namespace enigma::graphic
{
    class D12Texture;
}

class Geometry;

class Game
{
public:
    explicit Game();
    ~Game();
#pragma region LIFE_HOOK
    void Update();
    void Render();
#pragma endregion LIFE_HOOT

#pragma region SCENE
    std::unique_ptr<SceneUnitTest> m_scene           = nullptr;
    bool                           m_enableSceneTest = true;

    void UpdateScene();
    void RenderScene();
#pragma endregion

#pragma region RENDER_PASSES

public:
    std::unique_ptr<SceneRenderPass> m_skyRenderPass   = nullptr;
    std::unique_ptr<SceneRenderPass> m_cloudRenderPass = nullptr;

    std::unique_ptr<SceneRenderPass> m_compositeRenderPass = nullptr;
    std::unique_ptr<SceneRenderPass> m_finalRenderPass     = nullptr;

    std::unique_ptr<SceneRenderPass> m_debugRenderPass = nullptr;

    void RenderWorld();
    void RenderDebug();
#pragma endregion

#pragma region GAME_OBJECT
    std::unique_ptr<PlayerCharacter> m_player = nullptr; // player
#pragma endregion
#pragma region INPUT_ACTION

private:
    void ProcessInputAction(float deltaSeconds);
    void HandleESC();
#pragma endregion
#pragma region GAME_CLOCK
    std::unique_ptr<Clock> m_gameClock = nullptr;

public:
    const Clock* GetGameClock() const { return m_gameClock.get(); }
#pragma endregion
#pragma region TIME_OF_DAY

public:
    std::unique_ptr<TimeOfDayManager> m_timeOfDayManager = nullptr;
#pragma endregion
#pragma region IMGUI_SETTINGS

private:
    bool m_showGameSettings = false; // [Task 18] Toggle for ImGui Game Settings window (F1 key)
#pragma endregion
};
