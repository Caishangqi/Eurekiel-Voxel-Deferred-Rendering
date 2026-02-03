#include "TerrainRenderPass.hpp"

#include "Engine/Core/LogCategory/PredefinedCategories.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Core/Logger/LoggerSubsystem.hpp"
#include "Engine/Graphic/Bundle/ShaderBundle.hpp"
#include "Engine/Graphic/Bundle/Integration/ShaderBundleSubsystem.hpp"
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

TerrainRenderPass::TerrainRenderPass()
{
    // Load gbuffers_terrain shaders
    ShaderCompileOptions shaderCompileOptions;
    shaderCompileOptions.enableDebugInfo = true;

    m_shaderProgram = g_theShaderBundleSubsystem->GetCurrentShaderBundle()->GetProgram("gbuffers_terrain");

    // Get atlas image and create GPU texture (const_cast safe: CreateTexture2D only reads data)
    const Image* atlasImage = g_theResource->GetAtlas("blocks")->GetAtlasImage();
    if (atlasImage)
    {
        m_blockAtlasTexture = D3D12RenderSystem::CreateTexture2D(*const_cast<Image*>(atlasImage), TextureUsage::ShaderResource, "blockAtlas");
    }

    // Register Terrain vertex layout
    VertexLayoutRegistry::RegisterLayout(std::make_unique<TerrainVertexLayout>());
}

TerrainRenderPass::~TerrainRenderPass()
{
}

void TerrainRenderPass::Execute()
{
    BeginPass(); // [FIX] Set layout, shader, depth mode before rendering

    // Get world from game
    enigma::voxel::World* world = g_theGame ? g_theGame->GetWorld() : nullptr;
    if (!world)
    {
        return; // No world, skip render
    }

    g_theRendererSubsystem->SetCustomImage(0, m_blockAtlasTexture.get());

    const auto& chunks = world->GetLoadedChunks();
    for (auto it = chunks.begin(); it != chunks.end(); it++)
    {
        auto ChunkMesh = it->second->GetChunkMesh();
        if (!ChunkMesh || ChunkMesh->IsEmpty()) continue;
        if (it->second->GetState() != enigma::voxel::ChunkState::Active) continue;
        auto              opaqueTerrainVertexBuffer = ChunkMesh->GetOpaqueD12VertexBuffer();
        auto              opaqueTerrainIndexBuffer  = ChunkMesh->GetOpaqueD12IndexBuffer();
        PerObjectUniforms perObjectUniforms;
        perObjectUniforms.modelMatrix        = it->second->GetModelToWorldTransform();
        perObjectUniforms.modelMatrixInverse = perObjectUniforms.modelMatrix.GetInverse();
        Rgba8::WHITE.GetAsFloats(perObjectUniforms.modelColor);
        g_theRendererSubsystem->GetUniformManager()->UploadBuffer(perObjectUniforms);
        g_theRendererSubsystem->DrawVertexBuffer(opaqueTerrainVertexBuffer, opaqueTerrainIndexBuffer);
    }

    EndPass();
}

void TerrainRenderPass::BeginPass()
{
    // Set TerrainVertexLayout for terrain rendering
    // Reference: SceneUnitTest_VertexLayoutRegistration.cpp:83
    g_theRendererSubsystem->SetVertexLayout(TerrainVertexLayout::Get());

    // [REFACTOR] Pair-based RT binding
    if (m_shaderProgram)
    {
        g_theRendererSubsystem->UseProgram(m_shaderProgram, {{RenderTargetType::ColorTex, 0}, {RenderTargetType::ColorTex, 1}, {RenderTargetType::ColorTex, 2}, {RenderTargetType::DepthTex, 0}});
    }

    // Set depth mode: write enabled for terrain
    g_theRendererSubsystem->SetDepthConfig(DepthConfig::Enabled());

    // [REFACTOR] Use UpdateMatrixUniforms() instead of deprecated GetMatricesUniforms()
    MatricesUniforms matricesUniforms;
    g_theGame->m_player->GetCamera()->UpdateMatrixUniforms(matricesUniforms);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(matricesUniforms);
}

void TerrainRenderPass::EndPass()
{
    // [NOTE] Depth copy moved to TerrainCutoutRenderPass::EndPass()
    // depthtex1 should contain Solid + Cutout depth (noTranslucents)
    // Reference: Iris pipeline - depth copy happens after all opaque terrain
}

void TerrainRenderPass::OnShaderBundleLoaded(enigma::graphic::ShaderBundle* newBundle)
{
    if (newBundle)
    {
        m_shaderProgram = newBundle->GetProgram("gbuffers_terrain");
    }
}

void TerrainRenderPass::OnShaderBundleUnloaded()
{
    m_shaderProgram = nullptr;
}
