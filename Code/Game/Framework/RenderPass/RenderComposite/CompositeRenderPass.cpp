#include "CompositeRenderPass.hpp"

#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Graphic/Bundle/ShaderBundle.hpp"
#include "Engine/Graphic/Bundle/Integration/ShaderBundleSubsystem.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Graphic/Resource/VertexLayout/Layouts/Vertex_PCUTBNLayout.hpp"
#include "Engine/Graphic/Shader/Uniform/MatricesUniforms.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"
#include "Engine/Graphic/Target/DepthTextureProvider.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Framework/RenderPass/RenderPassHelper.hpp"
#include "Game/Gameplay/Game.hpp"

CompositeRenderPass::CompositeRenderPass()
{
    m_shaderPrograms = g_theShaderBundleSubsystem->GetCurrentShaderBundle()->GetPrograms("composite.*");
}

CompositeRenderPass::~CompositeRenderPass()
{
}

void CompositeRenderPass::Execute()
{
    BeginPass();
    for (auto program : m_shaderPrograms)
    {
        if (program)
        {
            auto rts = RenderPassHelper::GetRenderTargetColorFromIndex(program->GetDirectives().GetDrawBuffers(), RenderTargetType::ColorTex);
            g_theRendererSubsystem->UseProgram(program, rts);
            FullQuadsRenderer::DrawFullQuads();
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
}

void CompositeRenderPass::BeginPass()
{
    // [FIX] Transition depthtex0 to PIXEL_SHADER_RESOURCE for sampling
    // D3D12 Rule: Cannot read (SRV) and write (DSV) same resource simultaneously
    // [REFACTOR] Use D12DepthTexture::TransitionToShaderResource() per OCP
    auto* depthProvider = static_cast<enigma::graphic::DepthTextureProvider*>(g_theRendererSubsystem->GetRenderTargetProvider(RenderTargetType::DepthTex));
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
    auto* depthProvider = static_cast<enigma::graphic::DepthTextureProvider*>(g_theRendererSubsystem->GetRenderTargetProvider(RenderTargetType::DepthTex));
    depthProvider->GetDepthTexture(0)->TransitionToDepthWrite();
    depthProvider->GetDepthTexture(1)->TransitionToDepthWrite();
}
