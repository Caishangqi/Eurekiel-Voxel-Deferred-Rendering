/**
 * @file CloudRenderPass.cpp
 * @brief Cloud Rendering Pass Implementation (Minecraft Vanilla Style)
 * @date 2025-11-26
 */

#include "CloudRenderPass.hpp"
#include "CloudGeometryBuilder.hpp"
#include "Engine/Graphic/Resource/Buffer/D12VertexBuffer.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Framework/RenderPass/ConstantBuffer/CelestialConstantBuffer.hpp"
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

    // [Component 5.2] Generate cloud mesh using CloudGeometryBuilder (Fast mode)
    // Parameters: cellCount=32, cloudHeight=192.0f, fancyMode=false
    std::vector<Vertex> cloudVertices = CloudGeometryBuilder::GenerateCloudMesh(32, 192.0f, false);
    m_cloudVertexCount                = cloudVertices.size(); // Should be 6144 vertices (32*32*2*3)

    // [Component 3] Create and cache cloud mesh VertexBuffer
    size_t vertexDataSize = cloudVertices.size() * sizeof(Vertex);
    m_cloudMeshVB.reset(g_theRendererSubsystem->CreateVertexBuffer(vertexDataSize, sizeof(Vertex)));

    // [Component 3] Upload vertex data to GPU
    void* mappedData = m_cloudMeshVB->Map();
    memcpy(mappedData, cloudVertices.data(), vertexDataSize);
    m_cloudMeshVB->Unmap();
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

    // ==================== [Component 3] Upload CelestialConstantBuffer ====================
    // Cloud animation uses cloudTime from CelestialConstantBuffer
    CelestialConstantBuffer celestialData;
    celestialData.celestialAngle            = g_theGame->m_timeOfDayManager->GetCelestialAngle();
    celestialData.compensatedCelestialAngle = g_theGame->m_timeOfDayManager->GetCompensatedCelestialAngle();
    celestialData.cloudTime                 = g_theGame->m_timeOfDayManager->GetCloudTime();
    celestialData.sunPosition               = Vec3::ZERO; // Not used in cloud shader
    celestialData.moonPosition              = Vec3::ZERO; // Not used in cloud shader
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(celestialData);

    // ==================== [Component 3] Draw Cloud Mesh ====================
    std::vector<uint32_t> rtOutputs     = {0, 6, 3}; // colortex0, colortex6, colortex3
    int                   depthTexIndex = 0; // depthtex0
    g_theRendererSubsystem->UseProgram(m_cloudsShader, rtOutputs, depthTexIndex);

    // [Component 5.2] Draw cloud mesh (6144 vertices in Fast mode)
    g_theRendererSubsystem->DrawVertexBuffer(m_cloudMeshVB, m_cloudVertexCount);

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
    // ==================== [Component 3] Bind colortex0/6/3 as RenderTargets ====================
    std::vector<RTType> rtTypes    = {RTType::ColorTex, RTType::ColorTex, RTType::ColorTex};
    std::vector<int>    rtIndices  = {0, 6, 3};
    RTType              depthType  = RTType::DepthTex;
    uint32_t            depthIndex = 0;
    g_theRendererSubsystem->BindRenderTargets(rtTypes, rtIndices, depthType, depthIndex);

    // ==================== [Component 3] Set Depth State (LESS_EQUAL, no write) ====================
    // Clouds render after sky, depth test enabled but no depth write
    g_theRendererSubsystem->SetDepthMode(DepthMode::ReadOnly);

    // ==================== [Component 3] Enable Alpha Blending ====================
    // Clouds are semi-transparent, use standard alpha blending
    g_theRendererSubsystem->SetBlendMode(BlendMode::Alpha);
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
}

/**
 * @brief Switch between Fast/Fancy cloud rendering modes
 * 
 * Fast Mode: 6144 vertices (2 triangles per cell)
 * Fancy Mode: 24576 vertices (8 triangles per cell)
 * 
 * Note: Regenerates cloud mesh and recreates VertexBuffer
 */
void CloudRenderPass::SetFancyMode(bool fancy)
{
    if (m_fancyMode == fancy)
        return; // No change

    m_fancyMode = fancy;

    // [Component 5.2] Regenerate cloud mesh with new mode
    std::vector<Vertex> cloudVertices = CloudGeometryBuilder::GenerateCloudMesh(32, 192.0f, fancy);
    m_cloudVertexCount                = cloudVertices.size(); // 6144 (Fast) or 24576 (Fancy)

    // [Component 3] Recreate VertexBuffer
    size_t vertexDataSize = cloudVertices.size() * sizeof(Vertex);
    m_cloudMeshVB.reset(g_theRendererSubsystem->CreateVertexBuffer(vertexDataSize, sizeof(Vertex)));

    // [Component 3] Upload vertex data to GPU
    void* mappedData = m_cloudMeshVB->Map();
    memcpy(mappedData, cloudVertices.data(), vertexDataSize);
    m_cloudMeshVB->Unmap();
}
