#include "ShadowCompositeRenderPass.hpp"

#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Graphic/Bundle/Integration/ShaderBundleSubsystem.hpp"

void ShadowCompositeRenderPass::Execute()
{
    bool hasConfiguredPrograms = false;
    for (const auto& program : m_shaderPrograms)
    {
        if (program)
        {
            hasConfiguredPrograms = true;
            break;
        }
    }

    if (!hasConfiguredPrograms)
    {
        return;
    }

    ERROR_RECOVERABLE("ShadowCompositeRenderPass has shadowcomp.* programs but no draw path; wire explicit pass scope when enabling shadow composite draws");
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
    // Intentionally empty while ShadowComposite has no draw path.
    // If shadow composite rendering is implemented later, BeginPass must own an
    // explicit pass scope before any pass-scoped upload or draw submission.
}

void ShadowCompositeRenderPass::EndPass()
{
    // Intentionally empty while ShadowComposite has no draw path.
}
