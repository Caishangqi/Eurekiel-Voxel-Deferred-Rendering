#include "DeferredRenderPass.hpp"

#include "Engine/Graphic/Bundle/Integration/ShaderBundleSubsystem.hpp"
#include "Engine/Graphic/Bundle/ShaderBundle.hpp"
#include "Engine/Graphic/Core/RenderState/DepthState.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Graphic/Resource/VertexLayout/Layouts/Vertex_PCUTBNLayout.hpp"
#include "Engine/Graphic/Shader/Program/ShaderProgram.hpp"
#include "Engine/Graphic/Shader/Uniform/MatricesUniforms.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"
#include "Engine/Graphic/Target/D12RenderTarget.hpp"
#include "Engine/Graphic/Target/DepthTextureProvider.hpp"
#include "Engine/Graphic/Target/ShadowTextureProvider.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Framework/RenderPass/RenderPassHelper.hpp"
#include "Game/Gameplay/Game.hpp"
using namespace enigma::graphic;

DeferredRenderPass::DeferredRenderPass()
{
    m_shaderPrograms = g_theShaderBundleSubsystem->GetCurrentShaderBundle()->GetPrograms("deferred.*");
}

DeferredRenderPass::~DeferredRenderPass()
{
}

void DeferredRenderPass::Execute()
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

void DeferredRenderPass::BeginPass()
{
    // Disable depth testing/writing for full-screen quad pass
    g_theRendererSubsystem->SetDepthConfig(DepthConfig::Disabled());
    g_theRendererSubsystem->SetVertexLayout(Vertex_PCUTBNLayout::Get());

    // Transition depthtex0/1 to PIXEL_SHADER_RESOURCE for GBuffer sampling
    // D3D12 Rule: Cannot read (SRV) and write (DSV) same resource simultaneously
    auto* depthProvider = static_cast<DepthTextureProvider*>(g_theRendererSubsystem->GetRenderTargetProvider(RenderTargetType::DepthTex));
    depthProvider->GetDepthTexture(0)->TransitionToShaderResource();
    depthProvider->GetDepthTexture(1)->TransitionToShaderResource();

    // Transition shadowtex0/1 to PIXEL_SHADER_RESOURCE for shadow sampling
    auto* shadowProvider = static_cast<ShadowTextureProvider*>(g_theRendererSubsystem->GetRenderTargetProvider(RenderTargetType::ShadowTex));
    shadowProvider->GetDepthTexture(0)->TransitionToShaderResource();
    shadowProvider->GetDepthTexture(1)->TransitionToShaderResource();

    g_theRendererSubsystem->SetRasterizationConfig(RasterizationConfig::NoCull());

    // Upload uniforms needed by deferred lighting
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(MATRICES_UNIFORM);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(FOG_UNIFORM);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(WORLD_INFO_UNIFORM);

    // Bind stage-scoped custom textures for deferred pass
    // Save previous customImage state so global customTexture bindings survive
    auto* bundle = g_theShaderBundleSubsystem->GetCurrentShaderBundle().get();
    if (bundle && bundle->HasCustomTextures())
    {
        auto entries = bundle->GetCustomTexturesForStage("deferred");
        for (const auto& entry : entries)
        {
            m_savedCustomImages[entry.textureSlot] = g_theRendererSubsystem->GetCustomImage(entry.textureSlot);
            g_theRendererSubsystem->SetCustomImage(entry.textureSlot, entry.texture);
            g_theRendererSubsystem->SetSamplerConfig(entry.metadata.samplerSlot, entry.metadata.samplerConfig);
        }
    }
}

void DeferredRenderPass::EndPass()
{
    // Restore saved customImage bindings before other state cleanup
    for (const auto& [slotIndex, previousTexture] : m_savedCustomImages)
    {
        g_theRendererSubsystem->SetCustomImage(slotIndex, previousTexture);
    }
    m_savedCustomImages.clear();

    g_theRendererSubsystem->SetRasterizationConfig(RasterizationConfig::CullBack());

    // NOTE: Depth/shadow textures are NOT restored here.
    // CompositeRenderPass (composite1 VL) still needs them as SRV.
    // CompositeRenderPass::EndPass() handles the restore to DEPTH_WRITE.
}

void DeferredRenderPass::OnShaderBundleLoaded(enigma::graphic::ShaderBundle* newBundle)
{
    if (newBundle)
    {
        m_shaderPrograms = newBundle->GetPrograms("deferred.*");
    }
}

void DeferredRenderPass::OnShaderBundleUnloaded()
{
    m_shaderPrograms.clear();
}
