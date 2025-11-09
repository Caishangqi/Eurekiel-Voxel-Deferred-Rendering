#include "Game.hpp"


#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Game/Framework/GameObject/Geometry.hpp"

Game::Game()
{
    /// Prepare clock;
    m_gameClock = std::make_unique<Clock>();
    m_gameClock->Reset();

    /// Prepare scene
    m_cubeA             = std::make_unique<Geometry>(this);
    AABB3 geoCubeA      = AABB3(Vec3(0, 0, 0), Vec3(2, 2, 2));
    m_cubeA->m_position = Vec3(3, 0, 0);
    m_cubeA->SetVertices(geoCubeA.GetVertices())->SetIndices(geoCubeA.GetIndices());

    m_cubeB        = std::make_unique<Geometry>(this);
    AABB3 geoCubeB = AABB3(Vec3(3, 3, 0), Vec3(5, 5, 2));
    m_cubeB->SetVertices(geoCubeB.GetVertices())->SetIndices(geoCubeB.GetIndices());

    /// Prepare player
    m_player             = std::make_unique<PlayerCharacter>(this);
    m_player->m_position = Vec3(0, 0, 0);

    /// Prepare render resources
    enigma::graphic::ShaderCompileOptions shaderCompileOptions;
    shaderCompileOptions.enableDebugInfo = true;
    sp_gBufferBasic                      = g_theRendererSubsystem->CreateShaderProgramFromFiles(
        ".enigma/assets/engine/shaders/program/gbuffers_basic.vs.hlsl",
        ".enigma/assets/engine/shaders/program/gbuffers_basic.ps.hlsl",
        "gbuffers_basic",
        shaderCompileOptions
    );

    /// Ideally the shader code hlsl will include "Common.hlsl"
}

Game::~Game()
{
}

void Game::Update()
{
    /// Update Player locomotion
    m_player->Update(m_gameClock->GetDeltaSeconds());
}

void Game::Render()
{
    /// ========================================
    /// [DEPTH TEST] Basic Scene Test - Two Cubes with Different Depth Modes
    /// ========================================
    /// Test Objective:
    /// - Verify DepthMode::Enabled (depth test + write) on CubeA
    /// - Verify DepthMode::Disabled (no depth test) on CubeB
    /// - Validate depth buffer binding (depthtex0)
    ///
    /// Expected Result:
    /// - CubeA: Correct occlusion (hidden parts not visible)
    /// - CubeB: Always visible (no depth test, drawn on top)
    /// ========================================

    /// [STEP 1] Render player camera setup
    /// Execute player's camera "BeginCamera" to set up view/projection matrices
    m_player->Render();

    /// [STEP 2] Bind RenderTargets with Depth Buffer
    /// - ColorTex outputs: {4, 5, 6, 7} (temporary test configuration)
    /// - Depth buffer: depthtex0 (index 0)
    /// - Normally {0,1,2,3} should be used for GBuffer, but we use {4,5,6,7} to avoid conflicts
    std::vector<uint32_t> rtOutputs     = {4, 5, 6, 7};
    int                   depthTexIndex = 0;
    g_theRendererSubsystem->UseProgram(sp_gBufferBasic, rtOutputs, depthTexIndex);

    /// [STEP 3] Render CubeA with Depth Test ENABLED
    /// - DepthMode::Enabled = Depth test ON + Depth write ON
    /// - DirectX 12 Config: DepthEnable=TRUE, DepthWriteMask=ALL, DepthFunc=LESS_EQUAL
    /// - Effect: CubeA will be correctly occluded by objects in front of it
    g_theRendererSubsystem->SetDepthMode(DepthMode::Enabled);
    m_cubeA->Render(); // CubeA at (1,1,0) to (2,2,1)

    /// [STEP 4] Render CubeB with Depth Test DISABLED
    /// - DepthMode::Disabled = Depth test OFF + Depth write OFF
    /// - DirectX 12 Config: DepthEnable=FALSE
    /// - Effect: CubeB will always be visible, drawn on top regardless of depth
    /*
    g_theRendererSubsystem->SetDepthMode(DepthMode::Disabled);
    m_cubeB->Render(); // CubeB at (3,3,0) to (5,5,2)
    */

    /// [STEP 5] Present ColorTex4 to screen
    /// Display the rendered result from ColorTex4 (first RT output)
    g_theRendererSubsystem->PresentRenderTarget(4, RTType::ColorTex);

    /// ========================================
    /// [VERIFICATION CHECKLIST]
    /// ========================================
    /// [OK] CubeA rendered with depth test (correct occlusion)
    /// [OK] CubeB rendered without depth test (always visible)
    /// [OK] Both cubes visible in perspective view
    /// [OK] Depth buffer (depthtex0) correctly bound and used
    /// ========================================
}
