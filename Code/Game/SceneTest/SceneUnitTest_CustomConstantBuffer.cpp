#include "SceneUnitTest_CustomConstantBuffer.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Graphic/Target/RTTypes.hpp"
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

    /// Prepare scene - Three cubes for RGB multi-draw test
    /// [NEW] Left to right: Red (-4,0,0), Green (0,0,0), Blue (4,0,0)

    // [NEW] Red cube at left position
    m_cubeA             = std::make_unique<Geometry>(g_theGame);
    AABB3 geoCubeA      = AABB3(Vec3(0, 0, 0), Vec3(2, 2, 2));
    m_cubeA->m_position = Vec3(-4.0f, 0.0f, 0.0f);
    m_cubeA->m_color    = Rgba8::WHITE;
    m_cubeA->m_scale    = Vec3(1.0f, 1.0f, 1.0f);
    m_cubeA->SetVertices(geoCubeA.GetVertices())->SetIndices(geoCubeA.GetIndices());

    // [NEW] Green cube at center position
    m_cubeB             = std::make_unique<Geometry>(g_theGame);
    AABB3 geoCubeB      = AABB3(Vec3(0, 0, 0), Vec3(2, 2, 2));
    m_cubeB->m_position = Vec3(0.0f, 0.0f, 0.0f);
    m_cubeB->m_color    = Rgba8::WHITE;
    m_cubeB->m_scale    = Vec3(1.0f, 1.0f, 1.0f);
    m_cubeB->SetVertices(geoCubeB.GetVertices())->SetIndices(geoCubeB.GetIndices());

    // [NEW] Blue cube at right position
    m_cubeC             = std::make_unique<Geometry>(g_theGame);
    AABB3 geoCubeC      = AABB3(Vec3(0, 0, 0), Vec3(2, 2, 2));
    m_cubeC->m_position = Vec3(4.0f, 0.0f, 0.0f);
    m_cubeC->m_color    = Rgba8::WHITE;
    m_cubeC->m_scale    = Vec3(1.0f, 1.0f, 1.0f);
    m_cubeC->SetVertices(geoCubeC.GetVertices())->SetIndices(geoCubeC.GetIndices());

    /// Register Custom Constant Buffer
    /// [NEW] Slot 42, space=1 (Custom Buffer path via Descriptor Table)
    g_theRendererSubsystem->GetUniformManager()->RegisterBuffer<TestUserCustomUniform>(
        42, // slot
        UpdateFrequency::PerObject, // frequency
        BufferSpace::Custom, // space=1 (Custom)
        10000 // maxDraws
    );
}

SceneUnitTest_CustomConstantBuffer::~SceneUnitTest_CustomConstantBuffer()
{
}

void SceneUnitTest_CustomConstantBuffer::Update()
{
    // [NEW] Update is now empty - color upload happens per-draw in Render()
    // This tests Ring Buffer data independence: each draw gets its own color
}


void SceneUnitTest_CustomConstantBuffer::Render()
{
    auto* uniformMgr = g_theRendererSubsystem->GetUniformManager();

    // [REFACTOR] Setup shader and texture with pair-based RT binding
    g_theRendererSubsystem->SetCustomImage(0, tex_testUV.get());
    g_theRendererSubsystem->UseProgram(sp_gBufferTestCustomBuffer, {
                                           {RenderTargetType::ColorTex, 4}, {RenderTargetType::ColorTex, 5},
                                           {RenderTargetType::ColorTex, 6}, {RenderTargetType::ColorTex, 7},
                                           {RenderTargetType::DepthTex, 0}
                                       });

    // ========================================
    // [TEST] Multi-draw data independence verification
    // Expected: Red cube | Green cube | Blue cube (left to right)
    // Failure:  Blue cube | Blue cube | Blue cube (Ring Buffer isolation failed)
    // ========================================

    // [TEST] Draw 1: Red cube at (-4, 0, 0)
    m_userCustomUniform.color = Vec4(1.0f, 0.0f, 0.0f, 1.0f); // Red
    uniformMgr->UploadBuffer<TestUserCustomUniform>(m_userCustomUniform);
    m_cubeA->Render();

    // [TEST] Draw 2: Green cube at (0, 0, 0)
    m_userCustomUniform.color = Vec4(0.0f, 1.0f, 0.0f, 1.0f); // Green
    uniformMgr->UploadBuffer<TestUserCustomUniform>(m_userCustomUniform);
    m_cubeB->Render();

    // [TEST] Draw 3: Blue cube at (4, 0, 0)
    m_userCustomUniform.color = Vec4(0.0f, 0.0f, 1.0f, 1.0f); // Blue
    uniformMgr->UploadBuffer<TestUserCustomUniform>(m_userCustomUniform);
    m_cubeC->Render();

    // [PRESENT] Display result
    // ========================================
    g_theRendererSubsystem->PresentRenderTarget(4, RenderTargetType::ColorTex);
}
