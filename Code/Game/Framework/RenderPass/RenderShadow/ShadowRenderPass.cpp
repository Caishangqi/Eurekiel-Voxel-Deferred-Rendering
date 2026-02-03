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

using namespace enigma::graphic;

// ============================================================================
// Lifecycle
// ============================================================================

ShadowRenderPass::ShadowRenderPass()
{
    // [NEW] Load shadow shaders (like TerrainRenderPass:29)
    ShaderCompileOptions shaderCompileOptions;
    shaderCompileOptions.enableDebugInfo = true;

    m_shadowProgram = g_theShaderBundleSubsystem->GetCurrentShaderBundle()->GetProgram("shadow");

    // [NEW] Get atlas image for alpha testing in shadow pass
    const Image* atlasImage = g_theResource->GetAtlas("blocks")->GetAtlasImage();
    if (atlasImage)
    {
        m_blockAtlasTexture = D3D12RenderSystem::CreateTexture2D(
            *const_cast<Image*>(atlasImage),
            TextureUsage::ShaderResource,
            "shadowBlockAtlas"
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

    // [NEW] Update shadow camera matrices
    UpdateShadowCamera();

    // [NEW] Render terrain to shadow map
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
    // [NEW] Set TerrainVertexLayout for shadow rendering (same as terrain pass)
    g_theRendererSubsystem->SetVertexLayout(TerrainVertexLayout::Get());

    // [NEW] Use shadow program with shadowtex0 and shadowcolor0 render targets
    if (m_shadowProgram)
    {
        g_theRendererSubsystem->UseProgram(m_shadowProgram, {{RenderTargetType::ShadowTex, 0}, {RenderTargetType::ShadowColor, 0}});
    }

    // [NEW] Set depth mode for shadow pass
    g_theRendererSubsystem->SetDepthConfig(DepthConfig::Enabled());

    // [NEW] Set block atlas for alpha testing
    if (m_blockAtlasTexture)
    {
        g_theRendererSubsystem->SetCustomImage(0, m_blockAtlasTexture.get());
    }
}

void ShadowRenderPass::EndPass()
{
    // Shadow pass complete, depth written to shadowtex0
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

    float shadowAngle = timeProvider->GetShadowAngle();
    float yawDegrees  = shadowAngle * 360.0f + 90.0f;

    // Pitch: The elevation angle of the sun
    // When shadowAngle=0.25, the sun is at the zenith and the Shadow Camera looks down (pitch = +90)
    // When shadowAngle=0/0.5, the sun is on the horizon and the Shadow Camera looks horizontally (pitch = 0)
    // Use sin function: pitch = 90 * sin(shadowAngle * 2PI)
    // Note: sin(0.25 * 2PI) = sin(PI/2) = 1, so pitch = 90°
    float pitchDegrees = 90.0f * sinf(shadowAngle * 2.0f * PI);

    float rollDegrees = 0.0f;

    EulerAngles shadowOrientation(yawDegrees, pitchDegrees, rollDegrees);
    m_lightDirectionEulerAngles = shadowOrientation;
    Vec3 I, J, K;
    m_lightDirectionEulerAngles.GetAsVectors_IFwd_JLeft_KUp(I, J, K);
    m_lightDirection = I;

    m_shadowCamera->SetPositionAndOrientation(snappedPos, m_lightDirectionEulerAngles);

    MatricesUniforms shadowCameraUniforms = m_shadowCamera->GetMatrixUniforms();
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(shadowCameraUniforms);
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
