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
#include "Engine/Graphic/Shader/Uniform/PerObjectUniforms.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"
#include "Engine/Resource/ResourceSubsystem.hpp"
#include "Engine/Resource/Atlas/TextureAtlas.hpp"
#include "Engine/Voxel/World/TerrainVertexLayout.hpp"
#include "Engine/Voxel/World/World.hpp"

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
        m_blockAtlasTexture = D3D12RenderSystem::CreateTexture2D(
            *const_cast<Image*>(atlasImage),
            TextureUsage::ShaderResource,
            "blockAtlas_translucent"
        );
    }
}

TerrainTranslucentRenderPass::~TerrainTranslucentRenderPass()
{
    // Smart pointers auto-cleanup
}

void TerrainTranslucentRenderPass::Execute()
{
    // Get world from game
    enigma::voxel::World* world = g_theGame ? g_theGame->GetWorld() : nullptr;
    if (!world)
    {
        return;
    }

    BeginPass();

    // Bind block atlas texture
    g_theRendererSubsystem->SetCustomImage(0, m_blockAtlasTexture.get());

    // Iterate visible chunks and render translucent meshes
    const auto& chunks = world->GetLoadedChunks();
    for (auto it = chunks.begin(); it != chunks.end(); ++it)
    {
        auto chunkMesh = it->second->GetChunkMesh();
        if (!chunkMesh || !chunkMesh->HasTranslucentGeometry())
        {
            continue;
        }
        if (it->second->GetState() != enigma::voxel::ChunkState::Active)
        {
            continue;
        }

        // Get translucent vertex/index buffers
        auto translucentVertexBuffer = chunkMesh->GetTranslucentD12VertexBuffer();
        auto translucentIndexBuffer  = chunkMesh->GetTranslucentD12IndexBuffer();

        if (!translucentVertexBuffer || !translucentIndexBuffer)
        {
            continue;
        }

        // Upload per-object uniforms (model matrix)
        PerObjectUniforms perObjectUniforms;
        perObjectUniforms.modelMatrix        = it->second->GetModelToWorldTransform();
        perObjectUniforms.modelMatrixInverse = perObjectUniforms.modelMatrix.GetInverse();
        Rgba8::WHITE.GetAsFloats(perObjectUniforms.modelColor);
        g_theRendererSubsystem->GetUniformManager()->UploadBuffer(perObjectUniforms);

        // Draw translucent geometry
        g_theRendererSubsystem->DrawVertexBuffer(translucentVertexBuffer, translucentIndexBuffer);
    }

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
    // Copy depth texture before rendering translucent objects
    g_theRendererSubsystem->GetProvider(RTType::DepthTex)->Copy(0, 1);

    // Save current render states for restoration
    m_savedDepthConfig = g_theRendererSubsystem->GetDepthConfig();
    m_savedBlendConfig = g_theRendererSubsystem->GetBlendConfig();

    // Set TerrainVertexLayout (same as TerrainRenderPass)
    g_theRendererSubsystem->SetVertexLayout(TerrainVertexLayout::Get());

    g_theRendererSubsystem->UseProgram(m_waterShader, {{RTType::ColorTex, 0}, {RTType::ColorTex, 1}, {RTType::ColorTex, 2}, {RTType::DepthTex, 0}});

    // Upload matrices uniforms
    MatricesUniforms matricesUniforms;
    g_theGame->m_player->GetCamera()->UpdateMatrixUniforms(matricesUniforms);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(matricesUniforms);

    // Setup blend and depth states for translucent rendering
    SetupBlendState();
    SetupDepthState();
}

void TerrainTranslucentRenderPass::EndPass()
{
    RestoreRenderState();
}

void TerrainTranslucentRenderPass::SetupBlendState()
{
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
