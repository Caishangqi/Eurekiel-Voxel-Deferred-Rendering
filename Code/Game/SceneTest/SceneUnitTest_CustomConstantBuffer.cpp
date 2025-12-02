#include "SceneUnitTest_CustomConstantBuffer.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"
#include "Engine/Graphic/Sprite/Sprite.hpp"
#include "Engine/Graphic/Sprite/SpriteAtlas.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Framework/GameObject/Geometry.hpp"
#include "Game/Gameplay/Game.hpp"

SceneUnitTest_CustomConstantBuffer::SceneUnitTest_CustomConstantBuffer()
{
    /// Prepare render resources
    enigma::graphic::ShaderCompileOptions shaderCompileOptions;
    shaderCompileOptions.enableDebugInfo = true;


    sp_gBufferTestCustomBuffer = g_theRendererSubsystem->CreateShaderProgramFromFiles(
        ".enigma/assets/engine/shaders/develop/gbuffers_test_custom_buffer.vs.hlsl",
        ".enigma/assets/engine/shaders/develop/gbuffers_test_custom_buffer.ps.hlsl",
        "gbuffers_test_custom_buffer",
        shaderCompileOptions
    );

    tex_testCaizii = g_theRendererSubsystem->CreateTexture2D(
        ".enigma/assets/engine/textures/test/Caizii.png",
        TextureUsage::ShaderResource,
        "TestCaizii"
    );

    tex_testUV = g_theRendererSubsystem->CreateTexture2D(
        ".enigma/assets/engine/textures/test/TestUV.png",
        TextureUsage::ShaderResource,
        "TestUV"
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


    /// Register Custom Constant Buffer
    g_theRendererSubsystem->GetUniformManager()->RegisterBuffer<TestUserCustomUniform>(50, UpdateFrequency::PerObject);
}

SceneUnitTest_CustomConstantBuffer::~SceneUnitTest_CustomConstantBuffer()
{
}

void SceneUnitTest_CustomConstantBuffer::Update()
{
    m_userCustomUniform.dummySineValue = sinf(g_theGame->m_timeOfDayManager->GetCloudTime() * 100.f);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer<TestUserCustomUniform>(m_userCustomUniform);
}


void SceneUnitTest_CustomConstantBuffer::Render()
{
    std::vector<uint32_t> rtOutputs     = {4, 5, 6, 7};
    int                   depthTexIndex = 0;

    g_theRendererSubsystem->SetCustomImage(0, sa_testMoon->GetSprite(0).GetTexture().get());
    g_theRendererSubsystem->UseProgram(sp_gBufferTestCustomBuffer, rtOutputs, depthTexIndex);
    m_cubeC->Render();
    g_theRendererSubsystem->SetCustomImage(0, tex_testUV.get());
    m_cubeB->Render();

    // [PRESENT] Display result
    // ========================================
    g_theRendererSubsystem->PresentRenderTarget(4, RTType::ColorTex);
}
