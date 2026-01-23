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
#include "Game/Framework/RenderPass/RenderTerrain/TerrainRenderPass.hpp"
#include "Game/Framework/RenderPass/RenderTerrainCutout/TerrainCutoutRenderPass.hpp"
#include "Game/Framework/RenderPass/RenderTerrainTranslucent/TerrainTranslucentRenderPass.hpp"
#include "Game/SceneTest/SceneUnitTest_SpriteAtlas.hpp"
#include "Game/SceneTest/SceneUnitTest_StencilXRay.hpp"

// [Task 18] ImGui Integration
#include "Engine/Core/ImGui/ImGuiSubsystem.hpp"
#include "Engine/Core/LogCategory/PredefinedCategories.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"
#include "Engine/Model/ModelSubsystem.hpp"
#include "Engine/Registry/Block/BlockRegistry.hpp"
#include "Engine/Registry/Core/RegisterSubsystem.hpp"
#include "Engine/Voxel/Builtin/DefaultBlock.hpp"
#include "Game/Framework/Imgui/ImguiGameSettings.hpp"
#include "Game/Framework/Imgui/ImguiLeftDebugOverlay.hpp"
#include "Game/Framework/RenderPass/RenderDebug/DebugRenderPass.hpp"
#include "Game/Framework/RenderPass/RenderShadow/ShadowRenderPass.hpp"
#include "Game/SceneTest/SceneUnitTest_CustomConstantBuffer.hpp"
#include "Game/SceneTest/SceneUnitTest_VertexLayoutRegistration.hpp"
#include "Generator/SimpleMinerGenerator.hpp"

Game::Game()
{
    /// Set the game state

    // Set CursorMode
    g_theInput->SetCursorMode(CursorMode::FPS);

    /// Prepare clock;
    m_gameClock = std::make_unique<Clock>(Clock::GetSystemClock());
    m_gameClock->Unpause();

    /// Prepare WorldTimeProvider (replaces TimeOfDayManager)
    m_timeProvider = std::make_unique<enigma::voxel::WorldTimeProvider>();

    /// Prepare player
    m_player                = std::make_unique<PlayerCharacter>(this);
    m_player->m_position    = Vec3(-20, 0, 86);
    m_player->m_orientation = EulerAngles(-60, 24, 0);

    /// Scene (Test Only)
    m_scene = std::make_unique<SceneUnitTest_StencilXRay>();
    //m_scene = std::make_unique<SceneUnitTest_VertexLayoutRegistration>();
    //m_scene = std::make_unique<SceneUnitTest_CustomConstantBuffer>();

    /// Render Passes (Production)
    m_shadowRenderPass             = std::make_unique<ShadowRenderPass>();
    m_skyRenderPass                = std::make_unique<SkyRenderPass>();
    m_terrainRenderPass            = std::make_unique<TerrainRenderPass>();
    m_terrainCutoutRenderPass      = std::make_unique<TerrainCutoutRenderPass>(); // [NEW] Cutout terrain (leaves, grass)
    m_terrainTranslucentRenderPass = std::make_unique<TerrainTranslucentRenderPass>(); // [NEW] Translucent terrain (water)
    m_cloudRenderPass              = std::make_unique<CloudRenderPass>();
    m_compositeRenderPass          = std::make_unique<CompositeRenderPass>();
    m_finalRenderPass              = std::make_unique<FinalRenderPass>();

    /// Render Passes (Debug)
    m_debugRenderPass = std::make_unique<DebugRenderPass>();

    /// Block Registration Phase - MUST happen before World creation
    /// [NeoForge Pattern] Registration → Freeze → Compile
    RegisterBlocks();

    // [NEW] Freeze all registries after registration completes
    // Reference: NeoForge GameData.java:65-76
    auto* registerSubsystem = GEngine->GetSubsystem<enigma::core::RegisterSubsystem>();
    if (registerSubsystem)
    {
        registerSubsystem->FreezeAllRegistries();
    }

    // This applies blockstate rotations from JSON files
    auto* modelSubsystem = GEngine->GetSubsystem<enigma::model::ModelSubsystem>();
    if (modelSubsystem)
    {
        modelSubsystem->CompileAllBlockModels();
    }

    /// World Generator and World Creation
    using namespace enigma::voxel;

    auto generator = std::make_unique<SimpleMinerGenerator>();
    m_world        = std::make_unique<World>("world", 6693073380, std::move(generator));
    m_world->SetChunkActivationRange(settings.GetInt("video.simulationDistance", 6));

    /// Register ImGUI
    g_theImGui->RegisterWindow("GameSetting", [this]()
    {
        ImguiGameSettings::ShowWindow(&m_showGameSettings);
    });

    g_theImGui->RegisterWindow("DebugOverlay", [this]()
    {
        ImguiLeftDebugOverlay::ShowWindow(&m_showGameSettings);
    });

    /// Prepare Uniforms
    g_theRendererSubsystem->GetUniformManager()->RegisterBuffer<FogUniforms>(2, UpdateFrequency::PerFrame, BufferSpace::Custom);
    g_theRendererSubsystem->GetUniformManager()->RegisterBuffer<CommonConstantBuffer>(8, UpdateFrequency::PerObject, BufferSpace::Custom);
}

Game::~Game()
{
    // Save and close world before cleanup
    if (m_world)
    {
        LogInfo(LogGame, "Saving world before game shutdown...");
        m_world->SaveWorld();
        LogInfo(LogGame, "Initiating graceful shutdown...");
        m_world->PrepareShutdown(); // Stop new tasks
        m_world->WaitForPendingTasks(); // Wait for completion
        LogInfo(LogGame, "Closing world...");
        m_world->CloseWorld();
        m_world.reset();
    }
}

void Game::Update()
{
    if (!m_enableSceneTest) UpdateWorld();
    /// Update InputActions
    ProcessInputAction(m_gameClock->GetDeltaSeconds());
    /// Update Player locomotion
    m_player->Update(m_gameClock->GetDeltaSeconds());
    m_timeProvider->Update(m_gameClock->GetDeltaSeconds());

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

    m_shadowRenderPass->Execute();
    // [STEP 2] Sky Rendering (Must render FIRST, depth = 1.0)
    // Renders sky void gradient and sun/moon billboards to colortex0
    m_skyRenderPass->Execute();

    // [STEP 3] Terrain Rendering (G-Buffer deferred, normal depth)
    // Writes colortex0 (Albedo), colortex1 (Lightmap), colortex2 (Normal)
    m_terrainRenderPass->Execute();

    // [STEP 3.1] Terrain Cutout Rendering (leaves, grass, flowers)
    // Alpha-tested blocks with threshold 0.1 (Iris ONE_TENTH_ALPHA)
    // Same G-Buffer output as solid terrain
    m_terrainCutoutRenderPass->Execute();

    // [STEP 3.5] Translucent Terrain Rendering (Water, Ice, etc.)
    // Renders translucent terrain blocks with alpha blending
    // Uses depthtex1 for depth testing, writes to colortex0
    m_terrainTranslucentRenderPass->Execute();

    // [STEP 4] Cloud Rendering (Must render AFTER terrain, alpha blending)
    // Renders translucent clouds to colortex0 with alpha blending
    m_cloudRenderPass->Execute();

    m_compositeRenderPass->Execute();

    // [STEP 5] Final Pass
    // Samples colortex0 and outputs to backbuffer
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

void Game::RegisterBlocks()
{
    using namespace enigma::registry::block;

    LogInfo(LogGame, "Starting block registration phase...");

    std::filesystem::path dataPath      = ".enigma\\data";
    std::string           namespaceName = "simpleminer";

    BlockRegistry::LoadNamespaceBlocks(dataPath.string(), namespaceName);
    AIR = BlockRegistry::GetBlock("simpleminer", "air");
    LogInfo(LogGame, "Block registration completed!");
}

void Game::UpdateWorld()
{
    if (m_world)
    {
        // Sync player position to world and chunk manager for intelligent chunk loading
        if (m_player)
        {
            m_world->SetPlayerPosition(m_player->m_position);
        }

        m_world->Update(Clock::GetSystemClock().GetDeltaSeconds());
    }
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
