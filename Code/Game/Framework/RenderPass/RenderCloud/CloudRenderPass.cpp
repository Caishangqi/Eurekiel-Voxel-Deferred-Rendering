/**
 * @file CloudRenderPass.cpp
 * @brief Cloud Rendering Pass Implementation (Minecraft Vanilla Style)
 * @date 2025-11-26
 */

#include "CloudRenderPass.hpp"
#include "Engine/Graphic/Resource/Buffer/D12VertexBuffer.hpp"
#include "Engine/Graphic/Resource/Texture/D12Texture.hpp"
#include "Engine/Graphic/Shader/Uniform/PerObjectUniforms.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Framework/RenderPass/ConstantBuffer/CelestialConstantBuffer.hpp"
#include "Game/Framework/GameObject/PlayerCharacter.hpp"
#include "Game/Gameplay/Game.hpp"

/**
 * @brief CloudRenderPass Constructor
 * 
 * Initialization:
 * 1. Load gbuffers_clouds shader
 * 2. Generate cloud mesh using CloudGeometryBuilder (Fast mode by default)
 * 3. Create and cache VertexBuffer (6144 vertices)
 */
CloudRenderPass::CloudRenderPass()
{
    // [Component 3] Load gbuffers_clouds Shader
    enigma::graphic::ShaderCompileOptions shaderCompileOptions;
    shaderCompileOptions.enableDebugInfo = true;

    m_cloudsShader = g_theRendererSubsystem->CreateShaderProgramFromFiles(
        ".enigma/assets/engine/shaders/program/gbuffers_clouds.vs.hlsl",
        ".enigma/assets/engine/shaders/program/gbuffers_clouds.ps.hlsl",
        "gbuffers_clouds",
        shaderCompileOptions
    );


    // [Component 3] Create and cache cloud mesh VertexBuffer
    //size_t vertexDataSize = m_cloudVertices.size() * sizeof(Vertex);
    //m_cloudMeshVB.reset(g_theRendererSubsystem->CreateVertexBuffer(vertexDataSize, sizeof(Vertex)));

    m_cloudTexture = g_theRendererSubsystem->CreateTexture2D(
        ".enigma/assets/engine/textures/environment/clouds.png",
        TextureUsage::ShaderResource,
        "Sun Texture"
    );
}

CloudRenderPass::~CloudRenderPass()
{
}

/**
 * @brief Execute cloud rendering
 * 
 * Render Flow:
 * 1. BeginPass: Bind RTs, set depth/blend states
 * 2. Upload CelestialConstantBuffer (cloudTime for animation)
 * 3. Draw cloud mesh with gbuffers_clouds shader
 * 4. EndPass: Clean up states
 */
void CloudRenderPass::Execute()
{
    BeginPass();

    // ==================== [Component 5.2] Player Position Tracking ====================
    // Get current player position (Engine coordinate system: +X forward, +Y left, +Z up)
    Vec3 playerPos = g_theGame->m_player->m_position;

    // [SIMPLIFIED] Cloud grid is centered at origin (-192 to +192)
    // We simply translate it to player's XY position (Z stays at cloud height 192)
    // No need for complex chunk tracking - the grid follows player smoothly

    // ==================== [Component 3] Upload CelestialConstantBuffer ====================
    // Cloud animation uses cloudTime from CelestialConstantBuffer
    CelestialConstantBuffer celestialData;
    celestialData.celestialAngle            = g_theGame->m_timeOfDayManager->GetCelestialAngle();
    celestialData.compensatedCelestialAngle = g_theGame->m_timeOfDayManager->GetCompensatedCelestialAngle();
    celestialData.cloudTime                 = g_theGame->m_timeOfDayManager->GetCloudTime();
    celestialData.sunPosition               = Vec3::ZERO; // Not used in cloud shader
    celestialData.moonPosition              = Vec3::ZERO; // Not used in cloud shader
    celestialData.skyBrightness             = 1.0f; // Not used in cloud shader
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(celestialData);

    // ==================== [Component 3] Upload PerObjectUniforms (Cloud Transform) ====================
    // Cloud mesh follows player position (fractional offset for smooth movement)
    enigma::graphic::PerObjectUniforms cloudPerObj;

    // Build translation matrix - move cloud grid center to player XY position
    // Cloud grid is centered at origin, so translate to player position
    Mat44 cloudTransform;
    cloudTransform.SetTranslation3D(Vec3(playerPos.x, playerPos.y, 0.0f));

    cloudPerObj.modelMatrix        = cloudTransform;
    cloudPerObj.modelMatrixInverse = cloudTransform.GetOrthonormalInverse();
    cloudPerObj.modelColor[0]      = 1.0f;
    cloudPerObj.modelColor[1]      = 1.0f;
    cloudPerObj.modelColor[2]      = 1.0f;
    cloudPerObj.modelColor[3]      = m_cloudOpacity; // Use configurable opacity
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer<enigma::graphic::PerObjectUniforms>(cloudPerObj);

    // ==================== [Component 3] Draw Cloud Mesh ====================
    std::vector<uint32_t> rtOutputs     = {0, 6, 3}; // colortex0, colortex6, colortex3
    int                   depthTexIndex = 0; // depthtex0
    g_theRendererSubsystem->UseProgram(m_cloudsShader, rtOutputs, depthTexIndex);
    g_theRendererSubsystem->SetCustomImage(0, m_cloudTexture.get());
    // [Component 5.2] Draw cloud mesh with indexed vertices
    g_theRendererSubsystem->DrawVertexArray(m_cloudVertices, m_cloudIndices);

    EndPass();
}

/**
 * @brief Begin cloud rendering pass
 * 
 * Setup:
 * 1. Bind colortex0/6/3 as RenderTargets
 * 2. Set depth test: LESS_EQUAL (test enabled, write disabled)
 * 3. Enable alpha blending for semi-transparent clouds
 */
void CloudRenderPass::BeginPass()
{
    // ==================== [Component 3] Set Depth State (LESS_EQUAL, no write) ====================
    // Clouds render after sky, depth test enabled but no depth write
    g_theRendererSubsystem->SetDepthMode(DepthMode::ReadOnly);

    // ==================== [Component 3] Enable Alpha Blending ====================
    // Clouds are semi-transparent, use standard alpha blending
    g_theRendererSubsystem->SetBlendMode(BlendMode::Additive);

    // ==================== [Component 3] Set Rasterization Config (NoCull) ====================
    // Clouds need double-sided rendering (visible from above and below)
    g_theRendererSubsystem->SetRasterizationConfig(RasterizationConfig::NoCull());
}

/**
 * @brief End cloud rendering pass
 * 
 * Cleanup:
 * 1. Reset depth state to default
 * 2. Disable alpha blending
 * 3. Reset stencil state
 */
void CloudRenderPass::EndPass()
{
    // ==================== [Component 3] Reset Depth State ====================
    g_theRendererSubsystem->SetDepthMode(DepthMode::Enabled);

    // ==================== [Component 3] Disable Alpha Blending ====================
    g_theRendererSubsystem->SetBlendMode(BlendMode::Opaque);

    // ==================== [Component 3] Reset Rasterization Config ====================
    g_theRendererSubsystem->SetRasterizationConfig(RasterizationConfig::Default());
}

/**
 * @brief Rebuild cloud mesh (only updates UV offsets, vertex positions unchanged)
 * 
 * Called when player crosses chunk boundary (integer jump detection)
 * Updates UV coordinates based on current player chunk position
 * Vertex positions remain fixed at cloudHeight (192.0f)
 */
void CloudRenderPass::RebuildCloudMesh()
{
    // [Component 5.2] Regenerate cloud mesh with current fancy mode
    // NOTE: This only updates UV offsets based on player chunk position
    // Vertex positions stay fixed (clouds are always at height 192)
    m_cloudVertexCount = m_cloudVertices.size();
}

/**
 * @brief Switch between Fast/Fancy cloud rendering modes
 * 
 * Fast Mode: 6144 vertices (2 triangles per cell)
 * Fancy Mode: 24576 vertices (8 triangles per cell)
 * 
 * Note: Triggers complete mesh rebuild with new vertex count
 */
void CloudRenderPass::SetFancyMode(bool fancy)
{
    if (m_fancyMode == fancy)
        return; // No change

    m_fancyMode = fancy;

    // [Component 5.2] Trigger complete mesh rebuild
    RebuildCloudMesh();
}
