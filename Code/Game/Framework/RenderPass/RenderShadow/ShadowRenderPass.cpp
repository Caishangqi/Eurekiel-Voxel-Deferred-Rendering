#include "ShadowRenderPass.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Gameplay/Game.hpp"
#include "Game/Framework/GameObject/PlayerCharacter.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Graphic/Resource/Texture/D12Texture.hpp"
#include "Engine/Graphic/Resource/VertexLayout/VertexLayoutRegistry.hpp"
#include "Engine/Graphic/Shader/Uniform/MatricesUniforms.hpp"
#include "Engine/Graphic/Shader/Uniform/PerObjectUniforms.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"
#include "Engine/Graphic/Target/RTTypes.hpp"
#include "Engine/Resource/ResourceSubsystem.hpp"
#include "Engine/Resource/Atlas/TextureAtlas.hpp"
#include "Engine/Voxel/World/TerrainVertexLayout.hpp"
#include "Engine/Voxel/World/World.hpp"
#include "Engine/Math/MathUtils.hpp"

#include "Engine/Graphic/Bundle/ShaderBundle.hpp"
#include "Engine/Graphic/Bundle/Integration/ShaderBundleSubsystem.hpp"
#include "Engine/Graphic/Target/ShadowTextureProvider.hpp"

using namespace enigma::graphic;

// ============================================================================
// Lifecycle
// ============================================================================

ShadowRenderPass::ShadowRenderPass()
{
    // Load shadow shaders (like TerrainRenderPass:29)
    ShaderCompileOptions shaderCompileOptions;
    shaderCompileOptions.enableDebugInfo = true;

    m_shadowProgram = g_theShaderBundleSubsystem->GetCurrentShaderBundle()->GetProgram("shadow");

    // Get atlas image for alpha testing in shadow pass
    const Image* atlasImage = g_theResource->GetAtlas("blocks")->GetAtlasImage();
    if (atlasImage)
    {
        m_blockAtlasTexture = D3D12RenderSystem::CreateTexture2DWithMips(
            *const_cast<Image*>(atlasImage),
            TextureUsage::ShaderResource,
            "shadowBlockAtlas",
            4
        );
    }

    // [FIX] Create ShadowCamera once in constructor with default values
    Vec2 boundsMin(-SHADOW_HALF_PLANE, -SHADOW_HALF_PLANE);
    Vec2 boundsMax(SHADOW_HALF_PLANE, SHADOW_HALF_PLANE);

    m_shadowCamera = std::make_unique<ShadowCamera>(
        Vec3::ZERO, // Initial position (updated each frame)
        EulerAngles(), // Initial orientation (updated each frame)
        boundsMin,
        boundsMax,
        SHADOW_NEAR_PLANE,
        SHADOW_FAR_PLANE
    );
}

// ============================================================================
// SceneRenderPass Implementation
// ============================================================================

void ShadowRenderPass::Execute()
{
    BeginPass();

    // Update shadow camera matrices
    UpdateShadowCamera();

    // Render terrain to shadow map
    RenderShadowMap();

    EndPass();
}

void ShadowRenderPass::OnShaderBundleLoaded(enigma::graphic::ShaderBundle* newBundle)
{
    if (newBundle)
    {
        m_shadowProgram = newBundle->GetProgram("shadow");
    }
}

void ShadowRenderPass::OnShaderBundleUnloaded()
{
    m_shadowProgram = nullptr;
}

void ShadowRenderPass::BeginPass()
{
    // Set TerrainVertexLayout for shadow rendering (same as terrain pass)
    g_theRendererSubsystem->SetVertexLayout(TerrainVertexLayout::Get());

    // Use shadow program with shadowtex0, shadowcolor0, and shadowcolor1 render targets
    // shadow.ps.hlsl outputs: SV_TARGET0 -> shadowcolor0 (caustics), SV_TARGET1 -> shadowcolor1 (underwater VL)
    if (m_shadowProgram)
    {
        g_theRendererSubsystem->UseProgram(m_shadowProgram, {{RenderTargetType::ShadowTex, 0}, {RenderTargetType::ShadowColor, 0}, {RenderTargetType::ShadowColor, 1}});
    }

    // Set depth mode for shadow pass
    g_theRendererSubsystem->SetDepthConfig(DepthConfig::Enabled());

    // [FIX] Disable back-face culling for shadow pass — standard shadow mapping practice.
    // At low sun angles, terrain top faces are nearly edge-on to the shadow camera.
    // With back-face culling, these faces project to thin slivers with gaps (light leaks).
    // At sunAngle >= 0.5, top faces become back-facing and get culled entirely.
    //
    // [FIX] Hardware depth bias to prevent shadow acne (diagonal stripe self-shadowing).
    // Without bias, floating-point precision causes surfaces to intermittently shadow themselves.
    // slopeScaledDepthBias adapts to surface angle — steeper surfaces get more offset.
    // depthBiasClamp prevents excessive bias at extreme grazing angles (peter-panning).
    RasterizationConfig shadowRasterConfig  = RasterizationConfig::NoCull();
    shadowRasterConfig.depthBias            = 4000;
    shadowRasterConfig.slopeScaledDepthBias = 1.5f;
    shadowRasterConfig.depthBiasClamp       = 0.01f;
    g_theRendererSubsystem->SetRasterizationConfig(shadowRasterConfig);

    // Set block atlas for alpha testing
    if (m_blockAtlasTexture)
    {
        g_theRendererSubsystem->SetCustomImage(0, m_blockAtlasTexture.get());
    }

    // Bind stage-scoped custom textures for shadow pass (noise.png, cloud-water.png)
    // Save previous customImage state so global customTexture bindings survive
    auto* bundle = g_theShaderBundleSubsystem->GetCurrentShaderBundle().get();
    if (bundle && bundle->HasCustomTextures())
    {
        auto entries = bundle->GetCustomTexturesForStage("shadow");
        for (const auto& entry : entries)
        {
            m_savedCustomImages[entry.textureSlot] = g_theRendererSubsystem->GetCustomImage(entry.textureSlot);
            g_theRendererSubsystem->SetCustomImage(entry.textureSlot, entry.texture);
            g_theRendererSubsystem->SetSamplerConfig(entry.metadata.samplerSlot, entry.metadata.samplerConfig);
        }
    }

    // Set viewport to match shadow RT dimensions (may differ from swapchain size)
    auto* shadowTexProvider = dynamic_cast<enigma::graphic::ShadowTextureProvider*>(
        g_theRendererSubsystem->GetRenderTargetProvider(RenderTargetType::ShadowTex));
    if (shadowTexProvider)
    {
        g_theRendererSubsystem->SetViewport(shadowTexProvider->GetBaseWidth(), shadowTexProvider->GetBaseHeight());
    }
}

void ShadowRenderPass::EndPass()
{
    // Restore saved customImage bindings before other state cleanup
    for (const auto& [slotIndex, previousTexture] : m_savedCustomImages)
    {
        g_theRendererSubsystem->SetCustomImage(slotIndex, previousTexture);
    }
    m_savedCustomImages.clear();

    // Restore default back-face culling after shadow pass
    g_theRendererSubsystem->SetRasterizationConfig(RasterizationConfig::CullBack());

    // Restore viewport to swapchain dimensions after shadow pass
    const auto& config = g_theRendererSubsystem->GetConfiguration();
    g_theRendererSubsystem->SetViewport(config.renderWidth, config.renderHeight);
}

// ============================================================================
// Shadow Camera Management
// ============================================================================

void ShadowRenderPass::UpdateShadowCamera()
{
    Vec3 playerPos  = g_theGame->m_player->GetCamera()->GetPosition();
    Vec3 snappedPos = SnapToGrid(playerPos, SHADOW_INTERVAL_SIZE);

    auto& timeProvider = g_theGame->m_timeProvider;
    if (!timeProvider) return;

    // Get shadow light position in world space (pass identity matrix)
    Mat44 identity;
    Vec3  shadowLightPos = timeProvider->CalculateShadowLightPosition(identity);

    // Light direction = opposite of sun direction (light travels FROM sun TO scene)
    Vec3 sunDirection = shadowLightPos.GetNormalized();
    m_lightDirection  = -sunDirection;

    // Convert light direction to EulerAngles for shadow camera
    // Coordinate system: +X forward, +Y left, +Z up
    float yawDegrees       = Atan2Degrees(m_lightDirection.y, m_lightDirection.x);
    float horizontalLength = sqrtf(m_lightDirection.x * m_lightDirection.x +
        m_lightDirection.y * m_lightDirection.y);
    float pitchDegrees = Atan2Degrees(-m_lightDirection.z, horizontalLength);

    m_lightDirectionEulerAngles = EulerAngles(yawDegrees, pitchDegrees, 0.0f);
    m_shadowCamera->SetPositionAndOrientation(snappedPos, m_lightDirectionEulerAngles);

    // Update shadow matrices
    m_shadowCamera->UpdateMatrixUniforms(MATRICES_UNIFORM);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(MATRICES_UNIFORM);
}

void ShadowRenderPass::RenderShadowMap()
{
    // Get world from game
    enigma::voxel::World* world = g_theGame ? g_theGame->GetWorld() : nullptr;
    if (!world)
    {
        return; // No world, skip shadow render
    }

    const auto& chunks = world->GetLoadedChunks();

    // ========================================================================
    // Pass 1: Render Opaque + Cutout → shadowtex0
    // [Iris Ref] ShadowRenderer.java:464-469 - solid, cutout, cutoutMipped
    // ========================================================================
    for (auto it = chunks.begin(); it != chunks.end(); ++it)
    {
        auto chunkMesh = it->second->GetChunkMesh();
        if (!chunkMesh || chunkMesh->IsEmpty()) continue;
        if (it->second->GetState() != enigma::voxel::ChunkState::Active) continue;

        // Upload per-object uniforms (model matrix)
        PerObjectUniforms perObjectUniforms;
        perObjectUniforms.modelMatrix        = it->second->GetModelToWorldTransform();
        perObjectUniforms.modelMatrixInverse = perObjectUniforms.modelMatrix.GetInverse();
        Rgba8::WHITE.GetAsFloats(perObjectUniforms.modelColor);
        g_theRendererSubsystem->GetUniformManager()->UploadBuffer(perObjectUniforms);

        // Draw Opaque
        auto opaqueVertexBuffer = chunkMesh->GetOpaqueD12VertexBuffer();
        auto opaqueIndexBuffer  = chunkMesh->GetOpaqueD12IndexBuffer();
        if (opaqueVertexBuffer && opaqueIndexBuffer)
        {
            g_theRendererSubsystem->DrawVertexBuffer(opaqueVertexBuffer, opaqueIndexBuffer);
        }

        // Draw Cutout (hollow objects such as leaves)
        auto cutoutVertexBuffer = chunkMesh->GetCutoutD12VertexBuffer();
        auto cutoutIndexBuffer  = chunkMesh->GetCutoutD12IndexBuffer();
        if (cutoutVertexBuffer && cutoutIndexBuffer)
        {
            g_theRendererSubsystem->DrawVertexBuffer(cutoutVertexBuffer, cutoutIndexBuffer);
        }
    }

    // ========================================================================
    // Copy shadowtex0 → shadowtex1 (freeze pre-translucent depth)
    // [Iris Ref] ShadowRenderer.java:533 - copyPreTranslucentDepth()
    // [Iris Ref] ShadowRenderTargets.java:171-182 - GPU Blit operation
    // ========================================================================
    g_theRendererSubsystem->GetRenderTargetProvider(RenderTargetType::ShadowTex)->Copy(0, 1);

    // ========================================================================
    // Pass 2: Render Translucent → shadowtex0 only
    // [Iris Ref] ShadowRenderer.java:540-542 - translucent layer
    // shadowtex1 remains "frozen" with opaque+cutout depth only
    // ========================================================================
    for (auto it = chunks.begin(); it != chunks.end(); ++it)
    {
        auto chunkMesh = it->second->GetChunkMesh();
        if (!chunkMesh || chunkMesh->IsEmpty()) continue;
        if (it->second->GetState() != enigma::voxel::ChunkState::Active) continue;

        // Draw Translucent (translucent objects such as water)
        auto translucentVertexBuffer = chunkMesh->GetTranslucentD12VertexBuffer();
        auto translucentIndexBuffer  = chunkMesh->GetTranslucentD12IndexBuffer();
        if (translucentVertexBuffer && translucentIndexBuffer)
        {
            // Upload per-object uniforms (model matrix)
            PerObjectUniforms perObjectUniforms;
            perObjectUniforms.modelMatrix        = it->second->GetModelToWorldTransform();
            perObjectUniforms.modelMatrixInverse = perObjectUniforms.modelMatrix.GetInverse();
            Rgba8::WHITE.GetAsFloats(perObjectUniforms.modelColor);
            g_theRendererSubsystem->GetUniformManager()->UploadBuffer(perObjectUniforms);

            g_theRendererSubsystem->DrawVertexBuffer(translucentVertexBuffer, translucentIndexBuffer);
        }
    }
}

Vec3 ShadowRenderPass::SnapToGrid(const Vec3& pos, float interval)
{
    // Snap each component to nearest grid point
    // This prevents shadow swimming when camera moves slightly
    // Reference: design.md - intervalSize = 2.0f

    float halfInterval = interval * 0.5f;

    // Calculate offset from grid
    float offsetX = fmodf(pos.x + halfInterval, interval) - halfInterval;
    float offsetY = fmodf(pos.y + halfInterval, interval) - halfInterval;
    float offsetZ = fmodf(pos.z + halfInterval, interval) - halfInterval;

    // Snap to grid by removing offset
    return Vec3(
        pos.x - offsetX,
        pos.y - offsetY,
        pos.z - offsetZ
    );
}
