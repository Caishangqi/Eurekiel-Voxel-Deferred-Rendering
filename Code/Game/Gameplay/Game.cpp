#include "Game.hpp"


#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Game/Framework/GameObject/Geometry.hpp"

Game::Game()
{
    /// Prepare clock;
    m_gameClock = std::make_unique<Clock>();
    m_gameClock->Reset();

    /// Prepare scene
    m_cubeA        = std::make_unique<Geometry>(this);
    AABB3 geoCubeA = AABB3(Vec3(1, 1, 0), Vec3(2, 2, 1));
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
    /// Real Scene Test Foundation
    /// - Only test the basic rendering
    /// - render 2 cube with different size and enable depth test
    /// x not render texture

    /// Render player, especially execute player's camera "BeginCamera"
    m_player->Render();

    /// Before we render the geometry we first bind / Use shader (What is termilology in bindless render pipeline?)
    std::vector<uint32_t> rtOutputs = {0, 1, 2, 3};
    g_theRendererSubsystem->UseProgram(sp_gBufferBasic, rtOutputs); // We test rt out put, we fill rt 0 ,1, 2, 3
    m_cubeA->Render(); // Inside the render func, it should draw vertex
    m_cubeB->Render();

    /// [TEST] PresentRenderTarget functionality - display ColorTex0 to screen
    /// This replaces the previous PresentWithShader approach for testing
    g_theRendererSubsystem->PresentRenderTarget(0, RTType::ColorTex);

    /// The result of Scene Test fundation should have
    /// - 2 Cube display in perspective view
}
