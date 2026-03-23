#include "CompositeRenderPass.hpp"

#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Graphic/Bundle/ShaderBundle.hpp"
#include "Engine/Graphic/Bundle/Integration/ShaderBundleSubsystem.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Graphic/Resource/VertexLayout/Layouts/Vertex_PCUTBNLayout.hpp"
#include "Engine/Graphic/Shader/Uniform/MatricesUniforms.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"
#include "Engine/Graphic/Target/D12RenderTarget.hpp"
#include "Engine/Graphic/Target/DepthTextureProvider.hpp"
#include "Engine/Graphic/Target/ShadowTextureProvider.hpp"
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
            const auto& directives = program->GetDirectives();

            // Apply per-program blend from shaders.properties
            const auto& blendConfig = directives.GetBlendConfig();
            if (!blendConfig.IsUndefined())
            {
                g_theRendererSubsystem->SetBlendConfig(blendConfig);
            }

            // Apply per-buffer blend overrides
            const auto& bufferOverrides = directives.GetBufferBlendOverrides();
            for (const auto& [rtIndex, rtBlend] : bufferOverrides)
            {
                g_theRendererSubsystem->SetBlendConfig(rtBlend, rtIndex);
            }

            auto rts = RenderPassHelper::GetRenderTargetColorFromIndex(directives.GetDrawBuffers(), RenderTargetType::ColorTex);
            g_theRendererSubsystem->UseProgram(program, rts);
            FullQuadsRenderer::DrawFullQuads();

            // Reset blend state for next program
            g_theRendererSubsystem->SetBlendConfig(BlendConfig::Opaque());
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
    // [FIX] Transition depthtex0/1 to SRV for composite sampling (SSR, VL, etc.)
    // DeferredRenderPass::EndPass() restored them to DEPTH_WRITE for translucent passes
    auto* depthProvider = static_cast<enigma::graphic::DepthTextureProvider*>(g_theRendererSubsystem->GetRenderTargetProvider(RenderTargetType::DepthTex));
    depthProvider->GetDepthTexture(0)->TransitionToShaderResource();
    depthProvider->GetDepthTexture(1)->TransitionToShaderResource();

    // [FIX] Transition shadowtex0/1 to SRV for composite VL
    auto* shadowProvider = static_cast<enigma::graphic::ShadowTextureProvider*>(g_theRendererSubsystem->GetRenderTargetProvider(RenderTargetType::ShadowTex));
    shadowProvider->GetDepthTexture(0)->TransitionToShaderResource();
    shadowProvider->GetDepthTexture(1)->TransitionToShaderResource();

    g_theRendererSubsystem->SetDepthConfig(DepthConfig::Disabled());
    g_theRendererSubsystem->SetBlendConfig(BlendConfig::Opaque());
    g_theRendererSubsystem->SetVertexLayout(Vertex_PCUTBNLayout::Get());
    g_theRendererSubsystem->SetRasterizationConfig(RasterizationConfig::NoCull());

    // Upload uniforms needed by composite1 VL (shadow matrices, celestial data)
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(MATRICES_UNIFORM);

    // Bind stage-scoped custom textures for composite pass
    // Save previous customImage state so global customTexture bindings survive
    auto* bundle = g_theShaderBundleSubsystem->GetCurrentShaderBundle().get();
    if (bundle && bundle->HasCustomTextures())
    {
        auto entries = bundle->GetCustomTexturesForStage("composite");
        for (const auto& entry : entries)
        {
            m_savedCustomImages[entry.textureSlot] = g_theRendererSubsystem->GetCustomImage(entry.textureSlot);
            g_theRendererSubsystem->SetCustomImage(entry.textureSlot, entry.texture);
            g_theRendererSubsystem->SetSamplerConfig(entry.metadata.samplerSlot, entry.metadata.samplerConfig);
        }
    }
}

void CompositeRenderPass::EndPass()
{
    // Restore saved customImage bindings before other state cleanup
    for (const auto& [slotIndex, previousTexture] : m_savedCustomImages)
    {
        g_theRendererSubsystem->SetCustomImage(slotIndex, previousTexture);
    }
    m_savedCustomImages.clear();

    g_theRendererSubsystem->SetRasterizationConfig(RasterizationConfig::CullBack());

    // Restore depthtex0/1 to DEPTH_WRITE (transitioned to SRV by DeferredRenderPass)
    auto* depthProvider = static_cast<enigma::graphic::DepthTextureProvider*>(g_theRendererSubsystem->GetRenderTargetProvider(RenderTargetType::DepthTex));
    depthProvider->GetDepthTexture(0)->TransitionToDepthWrite();
    depthProvider->GetDepthTexture(1)->TransitionToDepthWrite();

    // Restore shadowtex0/1 to DEPTH_WRITE (transitioned to SRV by DeferredRenderPass)
    auto* shadowProvider = static_cast<enigma::graphic::ShadowTextureProvider*>(g_theRendererSubsystem->GetRenderTargetProvider(RenderTargetType::ShadowTex));
    shadowProvider->GetDepthTexture(0)->TransitionToDepthWrite();
    shadowProvider->GetDepthTexture(1)->TransitionToDepthWrite();
}
