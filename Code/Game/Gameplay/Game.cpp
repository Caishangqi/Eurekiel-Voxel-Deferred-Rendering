#include "Game.hpp"

#include <memory>
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Framework/App.hpp"
#include "Game/Framework/GameObject/Geometry.hpp"
#include "Game/Framework/RenderPass/RenderCloud/CloudRenderPass.hpp"
#include "Game/Framework/RenderPass/RenderComposite/CompositeRenderPass.hpp"
#include "Game/Framework/RenderPass/RenderFinal/FinalRenderPass.hpp"
#include "Game/Framework/RenderPass/RenderSky/SkyRenderPass.hpp"
#include "Game/SceneTest/SceneUnitTest_SpriteAtlas.hpp"
#include "Game/SceneTest/SceneUnitTest_StencilXRay.hpp"

// [Task 18] ImGui Integration
#include "Engine/Core/ImGui/ImGuiSubsystem.hpp"
#include "Game/Framework/Imgui/ImguiGameSettings.hpp"
#include "Game/Framework/Imgui/ImguiLeftDebugOverlay.hpp"
#include "Game/Framework/RenderPass/RenderDebug/DebugRenderPass.hpp"
#include "Game/SceneTest/SceneUnitTest_CustomConstantBuffer.hpp"

Game::Game()
{
    /// Set the game state

    // Set CursorMode
    g_theInput->SetCursorMode(CursorMode::FPS);

    /// Prepare clock;
    m_gameClock = std::make_unique<Clock>(Clock::GetSystemClock());
    m_gameClock->Unpause();

    /// Prepare TimeOfDayManager
    m_timeOfDayManager = std::make_unique<TimeOfDayManager>(m_gameClock.get());

    /// Prepare player
    m_player             = std::make_unique<PlayerCharacter>(this);
    m_player->m_position = Vec3(0, 0, 0);

    /// Scene (Test Only)
    //m_scene = std::make_unique<SceneUnitTest_StencilXRay>();
    m_scene = std::make_unique<SceneUnitTest_StencilXRay>();

    /// Render Passes (Production)
    m_skyRenderPass       = std::make_unique<SkyRenderPass>();
    m_cloudRenderPass     = std::make_unique<CloudRenderPass>();
    m_compositeRenderPass = std::make_unique<CompositeRenderPass>();
    m_finalRenderPass     = std::make_unique<FinalRenderPass>();

    /// Render Passes (Debug)
    m_debugRenderPass = std::make_unique<DebugRenderPass>();


    /// Register ImGUI
    g_theImGui->RegisterWindow("GameSetting", [this]()
    {
        ImguiGameSettings::ShowWindow(&m_showGameSettings);
    });

    g_theImGui->RegisterWindow("DebugOverlay", [this]()
    {
        ImguiLeftDebugOverlay::ShowWindow(&m_showGameSettings);
    });
}

Game::~Game()
{
}

void Game::Update()
{
    /// Update InputActions
    ProcessInputAction(m_gameClock->GetDeltaSeconds());
    /// Update Player locomotion
    m_player->Update(m_gameClock->GetDeltaSeconds());
    m_timeOfDayManager->Update();

#ifdef SCENE_TEST
    UpdateScene();
#endif
}

void Game::Render()
{
    // [STEP 1] Setup Camera (Player updates camera matrices)
    m_player->Render();
    if (!m_enableSceneTest)
    {
        RenderWorld();
        RenderDebug();

        /// Curretly presnet in here but later will move to Final Shader program's execute phases.
        /// TODO: Move it to Final Shader Program.
        g_theRendererSubsystem->PresentRenderTarget(0, RTType::ColorTex);
    }


#ifdef SCENE_TEST
    RenderScene();
#endif
}

void Game::RenderWorld()
{
    // ========================================
    // [Task 19] Deferred Rendering Pipeline Integration
    // Render Order: Sky → Terrain → Cloud → Final (Skip Composite)
    // ========================================


    // [STEP 2] Sky Rendering (Must render FIRST, depth = 1.0)
    // Renders sky void gradient and sun/moon billboards to colortex0
    m_skyRenderPass->Execute();

    // [STEP 3] Terrain/Scene Rendering (Middle layer, normal depth)
    // Renders opaque geometry (blocks, entities) to colortex0


    // [STEP 4] Cloud Rendering (Must render AFTER terrain, alpha blending)
    // Renders translucent clouds to colortex0 with alpha blending
    //m_cloudRenderPass->Execute();

    // [STEP 5] Final Pass (Skip Composite, RT Flipper not tested yet)
    // Samples colortex0 and outputs to backbuffer
    // TODO: Implement Composite Pass when RT Flipper mechanism is tested
    // m_compositeRenderPass->Execute(); // [DISABLED] RT Flipper未测试
    m_finalRenderPass->Execute();
}

void Game::RenderDebug()
{
    m_debugRenderPass->Execute();
}

void Game::ProcessInputAction(float deltaSeconds)
{
    UNUSED(deltaSeconds)
    if (g_theInput->WasKeyJustPressed(KEYCODE_ESC)) g_theApp->m_isQuitting = true;
    if (g_theInput->WasKeyJustPressed(KEYCODE_TILDE)) g_theInput->GetCursorMode() == CursorMode::POINTER ? g_theInput->SetCursorMode(CursorMode::FPS) : g_theInput->SetCursorMode(CursorMode::POINTER);

    // [Task 18] F1 key toggles ImGui Game Settings window
    if (g_theInput->WasKeyJustPressed(KEYCODE_F1))
    {
        m_showGameSettings = !m_showGameSettings;
    }
}

void Game::HandleESC()
{
}

void Game::UpdateScene()
{
    if (!m_enableSceneTest) return;
    if (m_scene) m_scene->Update();
}

void Game::RenderScene()
{
    if (!m_enableSceneTest) return;
    if (m_scene) m_scene->Render();
}
