#include "SceneUnitTest_SpriteAtlas.hpp"

#include "Engine/Graphic/Resource/Texture/D12Texture.hpp"
#include "Engine/Graphic/Target/RTTypes.hpp"
#include "Engine/Graphic/Sprite/Sprite.hpp"
#include "Engine/Graphic/Sprite/SpriteAtlas.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Framework/GameObject/Geometry.hpp"

SceneUnitTest_SpriteAtlas::SceneUnitTest_SpriteAtlas()
{
    /// Prepare render resources
    enigma::graphic::ShaderCompileOptions shaderCompileOptions;
    shaderCompileOptions.enableDebugInfo = true;
    sp_gBufferBasic                      = g_theRendererSubsystem->CreateShaderProgramFromFiles(
        ".enigma/assets/engine/shaders/program/gbuffers_basic.vs.hlsl",
        ".enigma/assets/engine/shaders/program/gbuffers_basic.ps.hlsl",
        "gbuffers_basic",
        shaderCompileOptions
    );

    tex_testUV = g_theRendererSubsystem->CreateTexture2D(
        ".enigma/assets/engine/textures/test/TestUV.png",
        TextureUsage::ShaderResource,
        "TestUV"
    );

    tex_testCaizii = g_theRendererSubsystem->CreateTexture2D(
        ".enigma/assets/engine/textures/test/Caizii.png",
        TextureUsage::ShaderResource,
        "TestCaizii"
    );

    sa_testMoon = std::make_shared<SpriteAtlas>("testMoonPhase");
    sa_testMoon->BuildFromGrid(".enigma/assets/engine/textures/environment/moon_phases.png", IntVec2(4, 2));

    /// Prepare scene

    m_cubeC             = std::make_unique<Geometry>(g_theGame);
    AABB3 geoCubeC      = AABB3(Vec3(0, 0, 0), Vec3(2, 2, 2));
    m_cubeC->m_position = Vec3(0, 0, 0);
    m_cubeC->m_color    = Rgba8::WHITE;
    m_cubeC->m_scale    = Vec3(2.0f, 2.0f, 2.0f); // Make CubeB larger to create occlusion for X-Ray test
    m_cubeC->SetVertices(geoCubeC.GetVertices(Rgba8::WHITE, sa_testMoon->GetSprite(0).GetUVBounds()))->SetIndices(geoCubeC.GetIndices());

    m_cubeB             = std::make_unique<Geometry>(g_theGame);
    AABB3 geoCubeB      = AABB3(Vec3(0, 0, 0), Vec3(1, 1, 1));
    m_cubeB->m_position = Vec3(4, 4, 0);
    m_cubeB->m_color    = Rgba8::WHITE;
    m_cubeB->m_scale    = Vec3(2.0f, 2.0f, 2.0f); // Make CubeB larger to create occlusion for X-Ray test
    m_cubeB->SetVertices(geoCubeB.GetVertices())->SetIndices(geoCubeB.GetIndices());
}

SceneUnitTest_SpriteAtlas::~SceneUnitTest_SpriteAtlas()
{
}

void SceneUnitTest_SpriteAtlas::Update()
{
}


void SceneUnitTest_SpriteAtlas::Render()
{
    // [REFACTOR] Pair-based RT binding
    g_theRendererSubsystem->SetCustomImage(0, sa_testMoon->GetSprite(0).GetTexture().get());
    g_theRendererSubsystem->UseProgram(sp_gBufferBasic, {
                                           {RTType::ColorTex, 4}, {RTType::ColorTex, 5},
                                           {RTType::ColorTex, 6}, {RTType::ColorTex, 7},
                                           {RTType::DepthTex, 0}
                                       });
    m_cubeC->Render();
    g_theRendererSubsystem->SetCustomImage(0, tex_testUV.get());
    m_cubeB->Render();

    // [PRESENT] Display result
    // ========================================
    g_theRendererSubsystem->PresentRenderTarget(4, RTType::ColorTex);
}
