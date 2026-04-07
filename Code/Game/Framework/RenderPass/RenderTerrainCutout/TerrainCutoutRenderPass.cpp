/**
 * @file TerrainCutoutRenderPass.cpp
 * @brief Terrain Cutout Render Pass Implementation
 *
 * Renders alpha-tested terrain blocks (leaves, grass, flowers, etc.)
 *
 * Iris Pipeline Reference:
 * - WorldRenderingPhase: TERRAIN_CUTOUT (index 10)
 * - Executes after TERRAIN_SOLID, before TERRAIN_TRANSLUCENT
 * - Uses ChunkMesh::GetCutoutD12VertexBuffer()
 *
 * Alpha Test Configuration:
 * - Threshold: 0.1 (ONE_TENTH_ALPHA in Iris)
 * - Pixels with alpha < 0.1 are discarded in shader
 */

#include "TerrainCutoutRenderPass.hpp"

#include "Engine/Core/LogCategory/PredefinedCategories.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Graphic/Bundle/ShaderBundle.hpp"
#include "Engine/Graphic/Bundle/Integration/ShaderBundleSubsystem.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Graphic/Mipmap/MipmapConfig.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Graphic/Target/RTTypes.hpp"
#include "Engine/Graphic/Resource/Texture/D12Texture.hpp"
#include "Engine/Graphic/Resource/VertexLayout/VertexLayoutRegistry.hpp"
#include "Engine/Graphic/Shader/Uniform/MatricesUniforms.hpp"
#include "Engine/Graphic/Shader/Uniform/PerObjectUniforms.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"
#include "Engine/Resource/ResourceSubsystem.hpp"
#include "Engine/Resource/Atlas/TextureAtlas.hpp"
#include "Engine/Voxel/Chunk/ChunkBatchCollector.hpp"
#include "Engine/Voxel/Chunk/ChunkBatchRenderer.hpp"
#include "Engine/Voxel/World/TerrainVertexLayout.hpp"
#include "Engine/Voxel/World/World.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Framework/RenderPass/WorldRenderingPhase.hpp"
#include "Game/Gameplay/Game.hpp"

using namespace enigma::graphic;

TerrainCutoutRenderPass::TerrainCutoutRenderPass()
{
    // Load gbuffers_terrain_cutout shaders
    // Fallback chain: gbuffers_terrain_cutout -> gbuffers_terrain

    m_shaderProgram = g_theShaderBundleSubsystem->GetCurrentShaderBundle()->GetProgram("gbuffers_terrain_cutout");

    // Get atlas image and create GPU texture
    // [NOTE] Shared with TerrainRenderPass - consider caching in ResourceSubsystem
    const Image* atlasImage = g_theResource->GetAtlas("blocks")->GetAtlasImage();
    if (atlasImage)
    {
        m_blockAtlasTexture = D3D12RenderSystem::CreateTexture2DWithMips(
            *const_cast<Image*>(atlasImage),
            TextureUsage::ShaderResource,
            "blockAtlas_cutout",
            4,
            MipmapConfig::AlphaWeighted()
        );
    }

    LogInfo(LogRenderer, "TerrainCutoutRenderPass initialized (alpha test threshold: 0.1)");
}

TerrainCutoutRenderPass::~TerrainCutoutRenderPass()
{
}

void TerrainCutoutRenderPass::Execute()
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
        enigma::voxel::ChunkBatchLayer::Cutout);
    world->MutableChunkBatchStats().batchedDraws += enigma::voxel::ChunkBatchRenderer::Submit(collection);

    EndPass();
}

void TerrainCutoutRenderPass::OnShaderBundleLoaded(enigma::graphic::ShaderBundle* newBundle)
{
    if (newBundle)
    {
        m_shaderProgram = newBundle->GetProgram("gbuffers_terrain_cutout");
    }
}

void TerrainCutoutRenderPass::OnShaderBundleUnloaded()
{
    m_shaderProgram = nullptr;
}

void TerrainCutoutRenderPass::BeginPass()
{
    // Set TerrainVertexLayout (same as solid terrain)
    g_theRendererSubsystem->SetVertexLayout(TerrainVertexLayout::Get());

    // Bind shader program with G-Buffer render targets
    if (m_shaderProgram)
    {
        g_theRendererSubsystem->UseProgram(
            m_shaderProgram,
            {
                {RenderTargetType::ColorTex, 0}, // colortex0: Albedo
                {RenderTargetType::ColorTex, 1}, // colortex1: Lightmap
                {RenderTargetType::ColorTex, 2}, // colortex2: Normal
                {RenderTargetType::DepthTex, 0} // depthtex0: Depth
            }
        );
    }

    // Enable depth testing and writing (same as solid terrain)
    g_theRendererSubsystem->SetDepthConfig(DepthConfig::Enabled());

    SceneRenderPass::BeginPass();

    // [REFACTOR] Update only gbuffer matrices in global MATRICES_UNIFORM
    if (g_theGame && g_theGame->GetRenderCamera())
    {
        g_theGame->GetRenderCamera()->UpdateMatrixUniforms(MATRICES_UNIFORM);
        g_theRendererSubsystem->GetUniformManager()->UploadBuffer(MATRICES_UNIFORM);
    }

    COMMON_UNIFORM.renderStage = ToRenderStage(WorldRenderingPhase::TERRAIN_CUTOUT);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(COMMON_UNIFORM);

    g_theRendererSubsystem->SetCustomImage(0, m_blockAtlasTexture.get());
}

void TerrainCutoutRenderPass::EndPass()
{
    SceneRenderPass::EndPass();

    // [FIX] Depth copy: depthtex0 -> depthtex1 (noTranslucents)
    // Must happen AFTER cutout rendering so depthtex1 contains Solid + Cutout depth
    // Reference: Iris pipeline - depthtex1 = depth without translucent objects
    g_theRendererSubsystem->GetRenderTargetProvider(RenderTargetType::DepthTex)->Copy(0, 1);
    LogDebug(LogRenderer, "TerrainCutoutRenderPass: Depth copied depthtex0 -> depthtex1 (noTranslucents)");
}
