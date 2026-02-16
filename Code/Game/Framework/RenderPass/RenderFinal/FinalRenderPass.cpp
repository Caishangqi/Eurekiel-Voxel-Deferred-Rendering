#include "FinalRenderPass.hpp"

#include "Engine/Graphic/Bundle/Integration/ShaderBundleSubsystem.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Graphic/Resource/VertexLayout/Layouts/Vertex_PCUTBNLayout.hpp"
#include "Engine/Graphic/Shader/Program/ShaderProgram.hpp"
#include "Game/Framework/RenderPass/RenderPassHelper.hpp"

FinalRenderPass::FinalRenderPass()
{
    g_theShaderBundleSubsystem->GetCurrentShaderBundle()->GetPrograms("final");
}

FinalRenderPass::~FinalRenderPass()
{
}

void FinalRenderPass::Execute()
{
    BeginPass();
    /// Logic In Here

    if (m_shadowProgram)
    {
        auto rts = RenderPassHelper::GetRenderTargetColorFromIndex(m_shadowProgram->GetDirectives().GetDrawBuffers(), RenderTargetType::ColorTex);
        g_theRendererSubsystem->UseProgram(m_shadowProgram, rts);
        FullQuadsRenderer::DrawFullQuads();
    }

    EndPass();
}

void FinalRenderPass::BeginPass()
{
    g_theRendererSubsystem->SetDepthConfig(DepthConfig::Disabled());
    g_theRendererSubsystem->SetVertexLayout(Vertex_PCUTBNLayout::Get());
    g_theRendererSubsystem->SetRasterizationConfig(RasterizationConfig::NoCull());
}

void FinalRenderPass::EndPass()
{
    g_theRendererSubsystem->SetRasterizationConfig(RasterizationConfig::CullBack());
}

void FinalRenderPass::OnShaderBundleLoaded(enigma::graphic::ShaderBundle* newBundle)
{
    if (newBundle)
    {
        m_shadowProgram = newBundle->GetProgram("final");
    }
}

void FinalRenderPass::OnShaderBundleUnloaded()
{
    m_shadowProgram.reset();
}
