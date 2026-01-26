#include "SceneRenderPass.hpp"
#include "Engine/Graphic/Bundle/ShaderBundleEvents.hpp"
#include "Engine/Graphic/Bundle/Integration/ShaderBundleSubsystem.hpp"
#include "Engine/Graphic/Bundle/ShaderBundle.hpp"

// ----------------------------------------------------------------------------
SceneRenderPass::SceneRenderPass()
{
    SubscribeToShaderBundleEvents();
}

// ----------------------------------------------------------------------------
SceneRenderPass::~SceneRenderPass()
{
    UnsubscribeFromShaderBundleEvents();
}

// ----------------------------------------------------------------------------
void SceneRenderPass::SubscribeToShaderBundleEvents()
{
    using namespace enigma::graphic;

    m_loadedHandle = ShaderBundleEvents::OnBundleLoaded.Add(
        this, &SceneRenderPass::OnShaderBundleLoaded);

    m_unloadedHandle = ShaderBundleEvents::OnBundleUnloaded.Add(
        this, &SceneRenderPass::OnShaderBundleUnloaded);
}

// ----------------------------------------------------------------------------
void SceneRenderPass::UnsubscribeFromShaderBundleEvents()
{
    using namespace enigma::graphic;

    if (m_loadedHandle != 0)
    {
        ShaderBundleEvents::OnBundleLoaded.Remove(m_loadedHandle);
        m_loadedHandle = 0;
    }

    if (m_unloadedHandle != 0)
    {
        ShaderBundleEvents::OnBundleUnloaded.Remove(m_unloadedHandle);
        m_unloadedHandle = 0;
    }
}

// ----------------------------------------------------------------------------
void SceneRenderPass::OnShaderBundleLoaded(enigma::graphic::ShaderBundle* /*newBundle*/)
{
    // Default: no-op. Subclasses override to cache ShaderProgram pointers.
}

// ----------------------------------------------------------------------------
void SceneRenderPass::OnShaderBundleUnloaded()
{
    // Default: no-op. Subclasses override to clear cached pointers.
}

// ----------------------------------------------------------------------------
std::shared_ptr<enigma::graphic::ShaderProgram> SceneRenderPass::GetProgramFromCurrentBundle(const std::string& programName)
{
    if (g_theShaderBundleSubsystem)
    {
        enigma::graphic::ShaderBundle* bundle = g_theShaderBundleSubsystem->GetCurrentShaderBundle().get();
        if (bundle)
        {
            return bundle->GetProgram(programName);
        }
    }
    return nullptr;
}
