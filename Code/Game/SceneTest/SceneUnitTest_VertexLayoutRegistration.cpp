#include "SceneUnitTest_VertexLayoutRegistration.hpp"

#include "Engine/Core/Logger/LoggerSubsystem.hpp"
#include "Engine/Graphic/Helper/VertexConversionHelper.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Graphic/Target/RTTypes.hpp"
#include "Engine/Graphic/Resource/Texture/D12Texture.hpp"
#include "Engine/Graphic/Resource/VertexLayout/Layouts/Vertex_PCULayout.hpp"
#include "Engine/Graphic/Resource/VertexLayout/Layouts/Vertex_PCUTBNLayout.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Framework/GameObject/Geometry.hpp"
#include "Game/Gameplay/Game.hpp"

SceneUnitTest_VertexLayoutRegistration::SceneUnitTest_VertexLayoutRegistration()
{
    m_cubeTexture = g_theRendererSubsystem->CreateTexture2D(
        ".enigma/assets/engine/textures/test/TestUV.png",
        enigma::graphic::TextureUsage::ShaderResource,
        "TestUV"
    );

    /// Prepare render resources
    enigma::graphic::ShaderCompileOptions shaderCompileOptions;
    shaderCompileOptions.enableDebugInfo = true;

#pragma region CUBE_1
    m_cube1ShaderProgram = g_theRendererSubsystem->CreateShaderProgramFromFiles(
        ".enigma/assets/engine/shaders/develop/gbuffers_test_vertex_layout_pcutbn.vs.hlsl",
        ".enigma/assets/engine/shaders/develop/gbuffers_test_vertex_layout_pcutbn.ps.hlsl",
        "gbuffers_test_vertex_layout_pcutbn",
        shaderCompileOptions
    );

    AABB3 geoCubeA              = AABB3(Vec3(0, 0, 0), Vec3(2, 2, 2));
    m_cube1Geometry             = std::make_unique<Geometry>(g_theGame);
    m_cube1Geometry->m_position = Vec3(-4.0f, 0.0f, 0.0f);
    m_cube1Geometry->m_color    = Rgba8::WHITE;
    m_cube1Geometry->m_scale    = Vec3(1.0f, 1.0f, 1.0f);
    m_cube1Uniforms.modelMatrix = m_cube1Geometry->GetModelToWorldTransform();
    m_cube1Geometry->m_color.GetAsFloats(m_cube1Uniforms.modelColor);
    geoCubeA.BuildVertices(m_cube1Vertices, m_cube1Indices);

    m_cube1VertexBuffer = D3D12RenderSystem::CreateVertexBuffer(m_cube1Vertices.size() * sizeof(Vertex_PCUTBN), sizeof(Vertex_PCUTBN), m_cube1Vertices.data());
    m_cube1IndexBuffer  = D3D12RenderSystem::CreateIndexBuffer(m_cube1Indices.size() * sizeof(uint32_t), m_cube1Indices.data());
#pragma endregion
#pragma region CUBE_2
    m_cube2ShaderProgram = g_theRendererSubsystem->CreateShaderProgramFromFiles(
        ".enigma/assets/engine/shaders/develop/gbuffers_test_vertex_layout_pcu.vs.hlsl",
        ".enigma/assets/engine/shaders/develop/gbuffers_test_vertex_layout_pcu.ps.hlsl",
        "gbuffers_test_vertex_layout_pcu",
        shaderCompileOptions
    );

    AABB3 geoCubeB              = AABB3(Vec3(0, 0, 0), Vec3(2, 2, 2));
    m_cube2Geometry             = std::make_unique<Geometry>(g_theGame);
    m_cube2Geometry->m_position = Vec3(0.0f, 0.0f, 0.0f);
    m_cube2Geometry->m_color    = Rgba8::DEBUG_GREEN;
    m_cube2Geometry->m_scale    = Vec3(2.0f, 2.0f, 2.0f);
    m_cube2Uniforms.modelMatrix = m_cube2Geometry->GetModelToWorldTransform();
    m_cube2Geometry->m_color.GetAsFloats(m_cube2Uniforms.modelColor);
    std::vector<Vertex> tempVertices;
    geoCubeA.BuildVertices(tempVertices, m_cube2Indices);
    std::vector<Vertex_PCU> tempVerticesPCU = VertexConversionHelper::ToPCUVector(tempVertices);

    m_cube2VertexBuffer = D3D12RenderSystem::CreateVertexBuffer(tempVerticesPCU.size() * sizeof(Vertex_PCU), sizeof(Vertex_PCU), tempVerticesPCU.data());
    m_cube2IndexBuffer  = D3D12RenderSystem::CreateIndexBuffer(m_cube2Indices.size() * sizeof(uint32_t), m_cube2Indices.data());
#pragma endregion
#pragma region CUBE_3
    m_cube3Geometry             = std::make_unique<Geometry>(g_theGame);
    AABB3 geoCubeC              = AABB3(Vec3(-1, -1, -1), Vec3(1, 1, 1));
    m_cube3Geometry->m_position = Vec3(10.0f, 3.0f, 3.0f);
    m_cube3Geometry->m_color    = Rgba8::WHITE;
    m_cube3Geometry->m_scale    = Vec3(3.0f, 3.0f, 3.0f);
    m_cube3Geometry->SetVertices(geoCubeC.GetVertices())->SetIndices(geoCubeC.GetIndices());
#pragma endregion
}

SceneUnitTest_VertexLayoutRegistration::~SceneUnitTest_VertexLayoutRegistration()
{
}

void SceneUnitTest_VertexLayoutRegistration::Render()
{
    // [REFACTOR] Pair-based RT binding
    g_theRendererSubsystem->UseProgram(m_cube1ShaderProgram, {{RTType::ColorTex, 0}, {RTType::DepthTex, 0}});
    g_theRendererSubsystem->SetCustomImage(0, m_cubeTexture.get());
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(m_cube1Uniforms);
    g_theRendererSubsystem->DrawVertexBuffer(m_cube1VertexBuffer, m_cube1IndexBuffer);

    g_theRendererSubsystem->SetVertexLayout(Vertex_PCULayout::Get());
    g_theRendererSubsystem->UseProgram(m_cube2ShaderProgram, {{RTType::ColorTex, 0}, {RTType::DepthTex, 0}});
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(m_cube2Uniforms);
    g_theRendererSubsystem->DrawVertexBuffer(m_cube2VertexBuffer, m_cube2IndexBuffer);

    g_theRendererSubsystem->SetVertexLayout(Vertex_PCUTBNLayout::Get());
    g_theRendererSubsystem->UseProgram(m_cube1ShaderProgram, {{RTType::ColorTex, 0}, {RTType::DepthTex, 0}});
    m_cube3Geometry->Render();
    g_theRendererSubsystem->PresentRenderTarget(0, RTType::ColorTex);
}

void SceneUnitTest_VertexLayoutRegistration::Update()
{
    m_cube3Geometry->m_orientation.m_rollDegrees  += g_theGame->GetGameClock()->GetDeltaSeconds() * 50;
    m_cube3Geometry->m_orientation.m_pitchDegrees += g_theGame->GetGameClock()->GetDeltaSeconds() * 50;
    m_cube3Geometry->m_orientation.m_yawDegrees   += g_theGame->GetGameClock()->GetDeltaSeconds() * 50;
}
