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
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Graphic/Target/RTTypes.hpp"
#include "Engine/Graphic/Resource/Texture/D12Texture.hpp"
#include "Engine/Graphic/Resource/VertexLayout/VertexLayoutRegistry.hpp"
#include "Engine/Graphic/Shader/Uniform/MatricesUniforms.hpp"
#include "Engine/Graphic/Shader/Uniform/PerObjectUniforms.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"
#include "Engine/Resource/ResourceSubsystem.hpp"
#include "Engine/Resource/Atlas/TextureAtlas.hpp"
#include "Engine/Voxel/World/TerrainVertexLayout.hpp"
#include "Engine/Voxel/World/World.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Gameplay/Game.hpp"

using namespace enigma::graphic;

TerrainCutoutRenderPass::TerrainCutoutRenderPass()
{
    // Load gbuffers_terrain_cutout shaders
    // Fallback chain: gbuffers_terrain_cutout -> gbuffers_terrain
    ShaderCompileOptions shaderCompileOptions;
    shaderCompileOptions.enableDebugInfo = true;

    m_shaderProgram = g_theRendererSubsystem->CreateShaderProgramFromFiles(
        ".enigma/assets/engine/shaders/program/gbuffers_terrain_cutout.vs.hlsl",
        ".enigma/assets/engine/shaders/program/gbuffers_terrain_cutout.ps.hlsl",
        "gbuffers_terrain_cutout",
        shaderCompileOptions
    );

    // Get atlas image and create GPU texture
    // [NOTE] Shared with TerrainRenderPass - consider caching in ResourceSubsystem
    const Image* atlasImage = g_theResource->GetAtlas("blocks")->GetAtlasImage();
    if (atlasImage)
    {
        m_blockAtlasTexture = D3D12RenderSystem::CreateTexture2D(
            *const_cast<Image*>(atlasImage),
            TextureUsage::ShaderResource,
            "blockAtlas_cutout"
        );
    }

    LogInfo(LogRenderer, "TerrainCutoutRenderPass initialized (alpha test threshold: 0.1)");
}

TerrainCutoutRenderPass::~TerrainCutoutRenderPass()
{
}

void TerrainCutoutRenderPass::Execute()
{
    BeginPass();

    // Get world from game
    enigma::voxel::World* world = g_theGame ? g_theGame->GetWorld() : nullptr;
    if (!world)
    {
        return; // No world, skip render
    }

    g_theRendererSubsystem->SetCustomImage(0, m_blockAtlasTexture.get());

    const auto& chunks = world->GetLoadedChunks();
    for (auto it = chunks.begin(); it != chunks.end(); ++it)
    {
        auto chunkMesh = it->second->GetChunkMesh();
        if (!chunkMesh) continue;
        if (it->second->GetState() != enigma::voxel::ChunkState::Active) continue;

        // [KEY DIFFERENCE] Use Cutout buffers instead of Opaque
        if (!chunkMesh->HasCutoutGeometry()) continue;

        auto cutoutVertexBuffer = chunkMesh->GetCutoutD12VertexBuffer();
        auto cutoutIndexBuffer  = chunkMesh->GetCutoutD12IndexBuffer();

        if (!cutoutVertexBuffer || !cutoutIndexBuffer) continue;

        // Upload per-object uniforms (model matrix)
        PerObjectUniforms perObjectUniforms;
        perObjectUniforms.modelMatrix        = it->second->GetModelToWorldTransform();
        perObjectUniforms.modelMatrixInverse = perObjectUniforms.modelMatrix.GetInverse();
        Rgba8::WHITE.GetAsFloats(perObjectUniforms.modelColor);
        g_theRendererSubsystem->GetUniformManager()->UploadBuffer(perObjectUniforms);

        // Draw cutout geometry
        g_theRendererSubsystem->DrawVertexBuffer(cutoutVertexBuffer, cutoutIndexBuffer);
    }

    EndPass();
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
                {RTType::ColorTex, 0}, // colortex0: Albedo
                {RTType::ColorTex, 1}, // colortex1: Lightmap
                {RTType::ColorTex, 2}, // colortex2: Normal
                {RTType::DepthTex, 0} // depthtex0: Depth
            }
        );
    }

    // Enable depth testing and writing (same as solid terrain)
    g_theRendererSubsystem->SetDepthConfig(DepthConfig::Enabled());

    // Upload camera matrices
    MatricesUniforms matricesUniforms;
    g_theGame->m_player->GetCamera()->UpdateMatrixUniforms(matricesUniforms);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(matricesUniforms);
}

void TerrainCutoutRenderPass::EndPass()
{
    // [FIX] Depth copy: depthtex0 -> depthtex1 (noTranslucents)
    // Must happen AFTER cutout rendering so depthtex1 contains Solid + Cutout depth
    // Reference: Iris pipeline - depthtex1 = depth without translucent objects
    g_theRendererSubsystem->GetProvider(RTType::DepthTex)->Copy(0, 1);
    LogDebug(LogRenderer, "TerrainCutoutRenderPass: Depth copied depthtex0 -> depthtex1 (noTranslucents)");
}
