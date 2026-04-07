/**
 * @file TerrainTranslucentRenderPass.cpp
 * @brief Translucent Terrain Rendering Pass Implementation
 * @date 2026-01-17
 *
 * Implementation Notes:
 * - Depth copy happens in BeginPass() before any rendering
 * - Blend mode: ALPHA (src_alpha, one_minus_src_alpha)
 * - Depth test: ENABLED with LESS_EQUAL comparison
 * - Depth write: DISABLED (to prevent occluding other translucent objects)
 * - Shader fallback ensures water renders even without gbuffers_water
 *
 * Reference:
 * - TerrainRenderPass.cpp: Chunk iteration and rendering pattern
 * - CloudRenderPass.cpp: Render state management pattern
 */

#include "TerrainTranslucentRenderPass.hpp"

#include "Engine/Graphic/Bundle/ShaderBundle.hpp"
#include "Engine/Graphic/Bundle/Integration/ShaderBundleSubsystem.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Gameplay/Game.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Graphic/Target/DepthTextureProvider.hpp"
#include "Engine/Graphic/Target/RTTypes.hpp"
#include "Engine/Graphic/Shader/Uniform/MatricesUniforms.hpp"
#include "Game/Framework/RenderPass/RenderPassHelper.hpp"
#include "Engine/Graphic/Shader/Uniform/PerObjectUniforms.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"
#include "Engine/Resource/ResourceSubsystem.hpp"
#include "Engine/Resource/Atlas/TextureAtlas.hpp"
#include "Engine/Voxel/Chunk/ChunkBatchCollector.hpp"
#include "Engine/Voxel/Chunk/ChunkBatchRenderer.hpp"
#include "Engine/Voxel/World/TerrainVertexLayout.hpp"
#include "Engine/Voxel/World/World.hpp"
#include "Game/Framework/RenderPass/WorldRenderingPhase.hpp"

using namespace enigma::graphic;

TerrainTranslucentRenderPass::TerrainTranslucentRenderPass()
{
    ShaderCompileOptions shaderCompileOptions;
    shaderCompileOptions.enableDebugInfo = true;

    // Load gbuffers_water shader (primary)
    m_waterShader = g_theShaderBundleSubsystem->GetCurrentShaderBundle()->GetProgram("gbuffers_water");

    // Load block atlas texture (same as TerrainRenderPass)
    const Image* atlasImage = g_theResource->GetAtlas("blocks")->GetAtlasImage();
    if (atlasImage)
    {
        m_blockAtlasTexture = D3D12RenderSystem::CreateTexture2DWithMips(
            *const_cast<Image*>(atlasImage),
            TextureUsage::ShaderResource,
            "blockAtlas_translucent",
            4
        );
    }
}

TerrainTranslucentRenderPass::~TerrainTranslucentRenderPass()
{
    // Smart pointers auto-cleanup
}

void TerrainTranslucentRenderPass::Execute()
{
    enigma::voxel::World* world = g_theGame ? g_theGame->GetWorld() : nullptr;
    if (!world)
    {
        return;
    }

    BeginPass();

    enigma::voxel::ChunkBatchViewContext viewContext;
    viewContext.world  = world;
    viewContext.camera = g_theGame ? g_theGame->GetChunkBatchCullingCamera() : nullptr;

    const enigma::voxel::ChunkBatchCollection       collection = enigma::voxel::ChunkBatchCollector::Collect(
        viewContext,
        enigma::voxel::ChunkBatchLayer::Translucent);
    world->MutableChunkBatchStats().batchedDraws += enigma::voxel::ChunkBatchRenderer::Submit(collection);

    EndPass();
}

void TerrainTranslucentRenderPass::OnShaderBundleLoaded(enigma::graphic::ShaderBundle* newBundle)
{
    if (newBundle)
    {
        m_waterShader = newBundle->GetProgram("gbuffers_water");
    }
}

void TerrainTranslucentRenderPass::OnShaderBundleUnloaded()
{
    m_waterShader = nullptr;
}

void TerrainTranslucentRenderPass::BeginPass()
{
    // [FIX] Depth copy (depthtex0 -> depthtex1) already done in TerrainCutoutRenderPass::EndPass()
    // No need to copy again here - DeferredRenderPass runs between cutout and translucent
    // but does not modify depthtex0 content (only transitions state)

    // Save current render states for restoration
    m_savedDepthConfig = g_theRendererSubsystem->GetDepthConfig();
    m_savedBlendConfig = g_theRendererSubsystem->GetBlendConfig();

    // Set TerrainVertexLayout (same as TerrainRenderPass)
    g_theRendererSubsystem->SetVertexLayout(TerrainVertexLayout::Get());

    // Bind render targets from shader RENDERTARGETS directive (e.g. 0,1,2,4)
    // Previously hardcoded {0,1,2} which missed colortex4 (material data for SSR)
    auto rts = RenderPassHelper::GetRenderTargetColorFromIndex(m_waterShader->GetDirectives().GetDrawBuffers(), RenderTargetType::ColorTex);
    rts.push_back({RenderTargetType::DepthTex, 0});
    g_theRendererSubsystem->UseProgram(m_waterShader, rts);

    SceneRenderPass::BeginPass();

    // [REFACTOR] Update only gbuffer matrices in global MATRICES_UNIFORM
    if (g_theGame && g_theGame->GetRenderCamera())
    {
        g_theGame->GetRenderCamera()->UpdateMatrixUniforms(MATRICES_UNIFORM);
        g_theRendererSubsystem->GetUniformManager()->UploadBuffer(MATRICES_UNIFORM);
    }

    COMMON_UNIFORM.renderStage = ToRenderStage(WorldRenderingPhase::TERRAIN_TRANSLUCENT);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(COMMON_UNIFORM);

    // Setup blend and depth states for translucent rendering
    SetupBlendState();
    SetupDepthState();

    g_theRendererSubsystem->SetCustomImage(0, m_blockAtlasTexture.get());

    // Bind stage-scoped custom textures for gbuffers_water (e.g. water normal texture on slot 3)
    auto* bundle = g_theShaderBundleSubsystem->GetCurrentShaderBundle().get();
    if (bundle && bundle->HasCustomTextures())
    {
        auto entries = bundle->GetCustomTexturesForStage("gbuffers_water");
        for (const auto& entry : entries)
        {
            m_savedCustomImages[entry.textureSlot] = g_theRendererSubsystem->GetCustomImage(entry.textureSlot);
            g_theRendererSubsystem->SetCustomImage(entry.textureSlot, entry.texture);
            g_theRendererSubsystem->SetSamplerConfig(entry.metadata.samplerSlot, entry.metadata.samplerConfig);
        }
    }
}

void TerrainTranslucentRenderPass::EndPass()
{
    SceneRenderPass::EndPass();

    // Restore saved customImage bindings
    for (const auto& [slotIndex, previousTexture] : m_savedCustomImages)
    {
        g_theRendererSubsystem->SetCustomImage(slotIndex, previousTexture);
    }
    m_savedCustomImages.clear();

    RestoreRenderState();
}

void TerrainTranslucentRenderPass::SetupBlendState()
{
    // Apply blend from shaders.properties if available, otherwise default alpha blend
    if (m_waterShader)
    {
        const auto& directives  = m_waterShader->GetDirectives();
        const auto& blendConfig = directives.GetBlendConfig();
        if (!blendConfig.IsUndefined())
        {
            g_theRendererSubsystem->SetBlendConfig(blendConfig);

            // Apply per-buffer blend overrides (e.g., colortex4=off for G-buffer data)
            const auto& bufferOverrides = directives.GetBufferBlendOverrides();
            for (const auto& [rtIndex, rtBlend] : bufferOverrides)
            {
                g_theRendererSubsystem->SetBlendConfig(rtBlend, rtIndex);
            }
            return;
        }
    }

    // Fallback: standard alpha blend
    g_theRendererSubsystem->SetBlendConfig(BlendConfig::Alpha());
}

void TerrainTranslucentRenderPass::SetupDepthState()
{
    // [FIX] Translucent rendering: depth test AND depth write enabled
    // Reference: Iris pipeline - translucent objects write to depthtex0
    // This allows composite shader to distinguish:
    // - depthtex0: all objects depth (including translucent)
    // - depthtex1: opaque-only depth (copied before translucent pass)
    // When depth0 < 1.0 && depth1 >= 1.0: translucent over sky
    DepthConfig translucentDepth;
    translucentDepth.depthTestEnabled  = true;
    translucentDepth.depthWriteEnabled = true; // [CRITICAL] Must write depth for depthtex0
    translucentDepth.depthFunc         = DepthComparison::LessEqual;
    g_theRendererSubsystem->SetDepthConfig(translucentDepth);
}

void TerrainTranslucentRenderPass::RestoreRenderState()
{
    g_theRendererSubsystem->SetDepthConfig(m_savedDepthConfig);
    g_theRendererSubsystem->SetBlendConfig(m_savedBlendConfig);
}
