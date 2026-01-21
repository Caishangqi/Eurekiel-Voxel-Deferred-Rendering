#pragma once
#include <memory>

#include "Engine/Core/Clock.hpp"
#include "Engine/Voxel/World/World.hpp"
#include "Game/Framework/GameObject/PlayerCharacter.hpp"
#include "Game/Framework/RenderPass/SceneRenderPass.hpp"
#include "Game/Framework/RenderPass/RenderTerrainTranslucent/TerrainTranslucentRenderPass.hpp"
#include "Engine/Voxel/Time/WorldTimeProvider.hpp"
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
    bool                           m_enableSceneTest = false;

    void UpdateScene();
    void RenderScene();
#pragma endregion
#pragma region RENDER_PASSES

public:
    std::unique_ptr<SceneRenderPass> m_shadowRenderPass             = nullptr;
    std::unique_ptr<SceneRenderPass> m_skyRenderPass                = nullptr;
    std::unique_ptr<SceneRenderPass> m_terrainRenderPass            = nullptr; // Terrain G-Buffer pass (solid)
    std::unique_ptr<SceneRenderPass> m_terrainCutoutRenderPass      = nullptr; // Terrain Cutout pass (leaves, grass)
    std::unique_ptr<SceneRenderPass> m_terrainTranslucentRenderPass = nullptr; // Translucent terrain (water)
    std::unique_ptr<SceneRenderPass> m_cloudRenderPass              = nullptr;

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
    std::unique_ptr<enigma::voxel::WorldTimeProvider> m_timeProvider = nullptr;
#pragma endregion
#pragma region IMGUI_SETTINGS

private:
    bool m_showGameSettings = false; // [Task 18] Toggle for ImGui Game Settings window (F1 key)
#pragma endregion
#pragma region RESOURCES
    void RegisterBlocks();

#pragma endregion
#pragma region WORLD

private:
    std::unique_ptr<enigma::voxel::World> m_world = nullptr;

public:
    enigma::voxel::World* GetWorld() const { return m_world.get(); }
    void                  UpdateWorld();
#pragma endregion
};
