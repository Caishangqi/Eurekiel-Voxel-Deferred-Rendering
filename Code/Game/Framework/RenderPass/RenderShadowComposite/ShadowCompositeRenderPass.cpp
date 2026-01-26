#include "ShadowCompositeRenderPass.hpp"

#include "Engine/Graphic/Bundle/Integration/ShaderBundleSubsystem.hpp"

void ShadowCompositeRenderPass::Execute()
{
    BeginPass();

    EndPass();
}

ShadowCompositeRenderPass::~ShadowCompositeRenderPass()
{
}

ShadowCompositeRenderPass::ShadowCompositeRenderPass()
{
    m_shaderPrograms = g_theShaderBundleSubsystem->GetCurrentShaderBundle()->GetPrograms("shadowcomp.*");
}

void ShadowCompositeRenderPass::OnShaderBundleLoaded(enigma::graphic::ShaderBundle* newBundle)
{
    if (newBundle)
    {
        m_shaderPrograms = newBundle->GetPrograms("shadowcomp.*");
    }
}

void ShadowCompositeRenderPass::OnShaderBundleUnloaded()
{
    m_shaderPrograms.clear();
}

void ShadowCompositeRenderPass::BeginPass()
{
}

void ShadowCompositeRenderPass::EndPass()
{
}
