#include "Game.hpp"

#include <memory>
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Graphic/Shader/Uniform/MatricesUniforms.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Framework/App.hpp"
#include "Game/Framework/GameObject/Geometry.hpp"
#include "Game/Framework/RenderPass/RenderCloud/CloudRenderPass.hpp"
#include "Game/Framework/RenderPass/RenderComposite/CompositeRenderPass.hpp"
#include "Game/Framework/RenderPass/RenderFinal/FinalRenderPass.hpp"
#include "Game/Framework/RenderPass/RenderSkyBasic/SkyBasicRenderPass.hpp"
#include "Game/Framework/RenderPass/RenderSkyTextured/SkyTexturedRenderPass.hpp"
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
#include "Game/Framework/RenderPass/RenderDeferred/DeferredRenderPass.hpp"
#include "Game/Framework/RenderPass/RenderShadow/ShadowRenderPass.hpp"
#include "Game/Framework/RenderPass/RenderShadowComposite/ShadowCompositeRenderPass.hpp"
#include "Generator/SimpleMinerGenerator.hpp"
#include "Generator/FlatWorldGenerator.hpp"
#include "ThirdParty/imgui/imgui.h"


CommonConstantBuffer COMMON_UNIFORM     = CommonConstantBuffer();
FogUniforms          FOG_UNIFORM        = FogUniforms();
WorldTimeUniforms    WORLD_TIME_UNIFORM = WorldTimeUniforms();
WorldInfoUniforms    WORLD_INFO_UNIFORM = WorldInfoUniforms();
MatricesUniforms     MATRICES_UNIFORM   = MatricesUniforms();


Game::Game()
{
    /// Set the game state

    // Set CursorMode
    g_theInput->SetCursorMode(CursorMode::POINTER);

    /// Prepare clock;
    m_gameClock = std::make_unique<Clock>(Clock::GetSystemClock());
    m_gameClock->Unpause();

    /// Prepare WorldTimeProvider (replaces TimeOfDayManager)
    m_timeProvider = std::make_unique<enigma::voxel::WorldTimeProvider>();

    /// Prepare player
    m_player                = std::make_unique<PlayerCharacter>(this);
    m_player->m_position    = Vec3(-20, 9, 75);
    m_player->m_orientation = EulerAngles(-160, -8, 0);

    /// Scene (Test Only)
    m_scene = std::make_unique<SceneUnitTest_StencilXRay>();
    //m_scene = std::make_unique<SceneUnitTest_VertexLayoutRegistration>();
    //m_scene = std::make_unique<SceneUnitTest_CustomConstantBuffer>();

    /// Render Passes (Production)
    m_shadowRenderPass             = std::make_unique<ShadowRenderPass>();
    m_shadowCompositeRenderPass    = std::make_unique<ShadowCompositeRenderPass>();
    m_skyBasicRenderPass           = std::make_unique<SkyBasicRenderPass>();
    m_skyTexturedRenderPass        = std::make_unique<SkyTexturedRenderPass>();
    m_terrainRenderPass            = std::make_unique<TerrainRenderPass>();
    m_terrainCutoutRenderPass      = std::make_unique<TerrainCutoutRenderPass>(); // Cutout terrain (leaves, grass)
    m_terrainTranslucentRenderPass = std::make_unique<TerrainTranslucentRenderPass>(); // Translucent terrain (water)
    m_cloudRenderPass              = std::make_unique<CloudRenderPass>();
    m_deferredRenderPass           = std::make_unique<DeferredRenderPass>();
    m_compositeRenderPass          = std::make_unique<CompositeRenderPass>();
    m_finalRenderPass              = std::make_unique<FinalRenderPass>();

    /// Render Passes (Debug)
    m_debugRenderPass = std::make_unique<DebugRenderPass>();

    /// Block Registration Phase - MUST happen before World creation
    /// [NeoForge Pattern] Registration → Freeze → Compile
    RegisterBlocks();

    // Freeze all registries after registration completes
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
    //auto generator = std::make_unique<FlatWorldGenerator>();
    m_world = std::make_unique<World>("world", 6693073380, std::move(generator));
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

    g_theImGui->RegisterWindow("Example", [this]()
    {
        bool show = true;
        ImGui::ShowDemoWindow(&show);
    });

    /// Prepare Uniforms
    g_theRendererSubsystem->GetUniformManager()->RegisterBuffer<FogUniforms>(2, UpdateFrequency::PerFrame, BufferSpace::Custom);
    g_theRendererSubsystem->GetUniformManager()->RegisterBuffer<CommonConstantBuffer>(8, UpdateFrequency::PerObject, BufferSpace::Custom);
    g_theRendererSubsystem->GetUniformManager()->RegisterBuffer<WorldTimeUniforms>(1, UpdateFrequency::PerFrame, BufferSpace::Custom);
    g_theRendererSubsystem->GetUniformManager()->RegisterBuffer<WorldInfoUniforms>(3, UpdateFrequency::PerFrame, BufferSpace::Custom);

    /// [R5.0] Subscribe to ShaderBundle lifecycle events for chunk mesh rebuild
    /// When a Bundle switches, material ID mappings change — all chunks must rebuild vertices
    /// (Reference: Iris levelRenderer.allChanged() on shader pack switch)
    m_onBundleLoadedHandle = enigma::graphic::ShaderBundleEvents::OnBundleLoaded.Add(
        [this](enigma::graphic::ShaderBundle*)
        {
            if (m_world)
                m_world->InvalidateAllChunkMeshes();
        }
    );
    m_onBundleUnloadedHandle = enigma::graphic::ShaderBundleEvents::OnBundleUnloaded.Add(
        [this]()
        {
            if (m_world)
                m_world->InvalidateAllChunkMeshes();
        }
    );
}

Game::~Game()
{
    // [R5.0] Unsubscribe from ShaderBundle events before world cleanup
    if (m_onBundleLoadedHandle != 0)
    {
        enigma::graphic::ShaderBundleEvents::OnBundleLoaded.Remove(m_onBundleLoadedHandle);
        m_onBundleLoadedHandle = 0;
    }
    if (m_onBundleUnloadedHandle != 0)
    {
        enigma::graphic::ShaderBundleEvents::OnBundleUnloaded.Remove(m_onBundleUnloadedHandle);
        m_onBundleUnloadedHandle = 0;
    }

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
    WORLD_TIME_UNIFORM.moonPhase = 1;
    WORLD_TIME_UNIFORM.worldDay  = m_timeProvider->GetDayCount();
    WORLD_TIME_UNIFORM.worldTime = m_timeProvider->GetCurrentTick();

    // Time counters for shader animation (Iris: CommonUniforms.java:118-119, SystemTimeUniforms.java:63)
    static int   s_frameCounter     = 0;
    static float s_frameTimeCounter = 0.0f;

    float deltaTime    = m_gameClock->GetDeltaSeconds();
    s_frameTimeCounter += deltaTime;
    if (s_frameTimeCounter >= 3600.0f)
        s_frameTimeCounter -= 3600.0f;
    s_frameCounter++;

    COMMON_UNIFORM.frameCounter     = s_frameCounter;
    COMMON_UNIFORM.frameTime        = deltaTime;
    COMMON_UNIFORM.frameTimeCounter = s_frameTimeCounter;

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
        /// Upload the Global Uniform
        g_theRendererSubsystem->GetUniformManager()->UploadBuffer(WORLD_TIME_UNIFORM);

        RenderWorld();
        RenderDebug();
        /// Curretly presnet in here but later will move to Final Shader program's execute phases.
        /// TODO: Move it to Final Shader Program.
        g_theRendererSubsystem->PresentRenderTarget(0, RenderTargetType::ColorTex);
    }


#ifdef SCENE_TEST
    RenderScene();
#endif
}

void Game::RenderWorld()
{
    // ========================================
    // Deferred Rendering Pipeline (Iris-compatible order)
    // Ref: IrisRenderingPipeline.java beginTranslucents() / finalizeLevelRendering()
    //
    // Order: Shadow → Sky → Opaque G-Buffer → Deferred Lighting
    //        → Translucent (water/cloud) → Composite → Final
    // ========================================

    // [STEP 1] Shadow pass
    m_shadowRenderPass->Execute();
    m_shadowCompositeRenderPass->Execute();

    // [STEP 2] Sky Rendering (depth = 1.0, rendered first into G-Buffer)
    m_skyBasicRenderPass->Execute();
    m_skyTexturedRenderPass->Execute();

    // [STEP 3] Opaque G-Buffer (terrain + cutout)
    // Writes colortex0 (Albedo), colortex1 (Lightmap), colortex2 (Normal)
    m_terrainRenderPass->Execute();
    m_terrainCutoutRenderPass->Execute();

    // [STEP 4] Deferred Lighting + Atmosphere
    // Full-screen pass: reads G-Buffer, outputs lit scene to colortex0
    // Must run BEFORE translucents so water/cloud blend onto the lit scene
    m_deferredRenderPass->Execute();

    // [STEP 5] Translucent Rendering (water, ice, clouds)
    // Runs AFTER deferred so colortex0 contains the lit scene for correct blending
    // Water writes colortex4 (material mask) for composite SSR detection
    m_terrainTranslucentRenderPass->Execute();
    m_cloudRenderPass->Execute();

    // [STEP 6] Composite passes (SSR, VL, tonemap)
    m_compositeRenderPass->Execute();

    // [STEP 7] Final output to backbuffer
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

    if (g_theInput->WasKeyJustPressed('K'))
    {
        auto m_stoneId  = enigma::registry::block::BlockRegistry::GetBlockId("simpleminer", "stone");
        auto stoneBlock = enigma::registry::block::BlockRegistry::GetBlockById(m_stoneId);
        m_world->SetBlockState(BlockPos(-20, 2, 65), stoneBlock->GetDefaultState());
        m_world->SetBlockState(BlockPos(-20, 2, 66), stoneBlock->GetDefaultState());
        m_world->SetBlockState(BlockPos(-20, 2, 67), stoneBlock->GetDefaultState());
        m_world->SetBlockState(BlockPos(-20, 2, 68), stoneBlock->GetDefaultState());
        m_world->SetBlockState(BlockPos(-20, 2, 69), stoneBlock->GetDefaultState());
        m_world->SetBlockState(BlockPos(-20, 2, 70), stoneBlock->GetDefaultState());
    }

    /// Reset Camera
    if (g_theInput->WasKeyJustPressed('R'))
    {
        m_player->m_position    = Vec3(-20, 9, 75);
        m_player->m_orientation = EulerAngles(-64, 33, 0);
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
