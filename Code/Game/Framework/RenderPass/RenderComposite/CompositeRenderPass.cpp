#include "CompositeRenderPass.hpp"

#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Graphic/Bundle/ShaderBundle.hpp"
#include "Engine/Graphic/Bundle/Integration/ShaderBundleSubsystem.hpp"
#include "Engine/Graphic/Helper/VertexConversionHelper.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Graphic/Resource/VertexLayout/Layouts/Vertex_PCUTBNLayout.hpp"
#include "Engine/Graphic/Resource/Buffer/D12VertexBuffer.hpp"
#include "Engine/Graphic/Shader/Common/ShaderCompilationHelper.hpp"
#include "Engine/Graphic/Shader/Uniform/MatricesUniforms.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"
#include "Engine/Graphic/Target/DepthTextureProvider.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Gameplay/Game.hpp"

CompositeRenderPass::CompositeRenderPass()
{
    enigma::graphic::ShaderCompileOptions shaderCompileOptions;
    shaderCompileOptions.enableDebugInfo = true;

    m_shaderProgram = g_theRendererSubsystem->CreateShaderProgramFromFiles(
        ".enigma/assets/engine/shaders/program/composite.vs.hlsl",
        ".enigma/assets/engine/shaders/program/composite.ps.hlsl",
        "composite",
        shaderCompileOptions
    );

    m_shaderPrograms = g_theShaderBundleSubsystem->GetCurrentShaderBundle()->GetPrograms("composite.*");

    /// Fullquads vertex buffer
    std::vector<Vertex_PCU> verts;
    AABB2                   fullQuadsAABB2;
    fullQuadsAABB2.m_mins = Vec2(-1, -1);
    fullQuadsAABB2.m_maxs = Vec2(1.0, 1.0f);
    AddVertsForAABB2D(verts, fullQuadsAABB2, Rgba8::WHITE);
    auto vertsTBN             = VertexConversionHelper::ToPCUTBNVector(verts);
    vertsTBN[0].m_uvTexCoords = Vec2(0.0f, 1.0f);
    vertsTBN[1].m_uvTexCoords = Vec2(1.0f, 0.0f);
    vertsTBN[2].m_uvTexCoords = Vec2(0.0f, 0.0f);
    vertsTBN[3].m_uvTexCoords = Vec2(0.0f, 1.0f);
    vertsTBN[4].m_uvTexCoords = Vec2(1.0f, 1.0f);
    vertsTBN[5].m_uvTexCoords = Vec2(1.0f, 0.0f);
    m_fullQuadsVertexBuffer   = D3D12RenderSystem::CreateVertexBuffer(vertsTBN.size() * sizeof(Vertex_PCUTBN), sizeof(Vertex_PCUTBN), vertsTBN.data());
}

CompositeRenderPass::~CompositeRenderPass()
{
}

void CompositeRenderPass::Execute()
{
    BeginPass();
    for (auto program : m_shaderPrograms)
    {
        if (m_shaderProgram)
        {
            g_theRendererSubsystem->UseProgram(m_shaderProgram, {{RTType::ColorTex, 0}});
            g_theRendererSubsystem->DrawVertexBuffer(m_fullQuadsVertexBuffer);
        }
    }
    EndPass();
}

void CompositeRenderPass::OnShaderBundleLoaded(enigma::graphic::ShaderBundle* newBundle)
{
    if (newBundle)
    {
        m_shaderPrograms = newBundle->GetPrograms("composite.*");
    }
}

void CompositeRenderPass::OnShaderBundleUnloaded()
{
    m_shaderPrograms.clear();
    m_shaderPrograms.push_back(m_shaderProgram);
}

void CompositeRenderPass::BeginPass()
{
    // [FIX] Transition depthtex0 to PIXEL_SHADER_RESOURCE for sampling
    // D3D12 Rule: Cannot read (SRV) and write (DSV) same resource simultaneously
    // [REFACTOR] Use D12DepthTexture::TransitionToShaderResource() per OCP
    auto* depthProvider = static_cast<enigma::graphic::DepthTextureProvider*>(g_theRendererSubsystem->GetProvider(RTType::DepthTex));
    g_theRendererSubsystem->SetDepthConfig(DepthConfig::Disabled());
    g_theRendererSubsystem->SetVertexLayout(Vertex_PCUTBNLayout::Get());
    depthProvider->GetDepthTexture(0)->TransitionToShaderResource();
    depthProvider->GetDepthTexture(1)->TransitionToShaderResource();
    g_theRendererSubsystem->SetRasterizationConfig(RasterizationConfig::NoCull());
    auto matrixUniform = g_theGame->m_player->GetCamera()->GetMatrixUniforms();
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(matrixUniform);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(FOG_UNIFORM);
}

void CompositeRenderPass::EndPass()
{
    g_theRendererSubsystem->SetRasterizationConfig(RasterizationConfig::CullBack());

    // [FIX] Transition depthtex0 back to DEPTH_WRITE for subsequent passes
    // [REFACTOR] Use D12DepthTexture::TransitionToDepthWrite() per OCP
    auto* depthProvider = static_cast<enigma::graphic::DepthTextureProvider*>(g_theRendererSubsystem->GetProvider(RTType::DepthTex));
    depthProvider->GetDepthTexture(0)->TransitionToDepthWrite();
    depthProvider->GetDepthTexture(1)->TransitionToDepthWrite();
}
