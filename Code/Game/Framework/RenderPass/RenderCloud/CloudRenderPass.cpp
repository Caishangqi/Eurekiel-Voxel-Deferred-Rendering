/**
 * @file CloudRenderPass.cpp
 * @brief Sodium-style Cloud Rendering Pass Implementation
 * @date 2025-12-02
 *
 * Architecture Overview:
 * - Reference: Sodium CloudRenderer.java (Line 70-528)
 * - CPU-side geometry generation with CloudTextureData and CloudGeometryHelper
 * - Geometry rebuilt only when player crosses cell boundaries
 * - ModelMatrix positions geometry in world space, centered on camera
 *
 * Coordinate System:
 * - Engine: +X Forward, +Y Left, +Z Up
 * - Cloud cells: 12x12 blocks horizontal, 4 blocks vertical
 *
 * [IMPORTANT] ModelMatrix Calculation:
 * - Geometry vertices are in LOCAL space: range [-radius*12, +radius*12]
 * - ModelMatrix translates to WORLD space: center at (cameraPos.x, cameraPos.y, CLOUD_HEIGHT)
 * - gbufferModelView then transforms to camera space
 * - This ensures clouds always follow the player correctly
 */

#include "CloudRenderPass.hpp"
#include "CloudTextureData.hpp"
#include "CloudGeometryHelper.hpp"
#include "Engine/Core/Image.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Graphic/Shader/Uniform/PerObjectUniforms.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Gameplay/Game.hpp"
#include "Game/Framework/GameObject/PlayerCharacter.hpp"
#include <cmath>

#include "Engine/Graphic/Bundle/Integration/ShaderBundleSubsystem.hpp"
#include "Engine/Graphic/Target/RTTypes.hpp"
#include "Engine/Graphic/Resource/VertexLayout/Layouts/Vertex_PCUTBNLayout.hpp"
#include "Engine/Graphic/Shader/Uniform/MatricesUniforms.hpp"

// ========================================
// Constants (Defaults, overridden by CloudConfig)
// ========================================

/// Cloud animation offset (Sodium uses 0.33 for Y-axis offset)
constexpr float CLOUD_OFFSET = 0.33f;

CloudRenderPass::CloudRenderPass()
    : m_needsRebuild(true)
{
    // Load config first (needed for render mode initialization)
    m_configParser = std::make_unique<CloudConfigParser>(".enigma/settings.yml");
    m_renderMode   = m_configParser->GetParsedConfig().renderMode;

    // Load gbuffers_clouds shader
    enigma::graphic::ShaderCompileOptions shaderCompileOptions;
    shaderCompileOptions.enableDebugInfo = true;

    m_cloudsShader = g_theShaderBundleSubsystem->GetCurrentShaderBundle()->GetProgram("gbuffers_clouds");

    // Load clouds.png and create CloudTextureData
    LoadCloudTexture();

    // Initialize empty CloudGeometry (will be built on first Execute)
    m_geometry = std::make_unique<CloudGeometry>();
}

CloudRenderPass::~CloudRenderPass()
{
    // Smart pointers auto-cleanup
}

void CloudRenderPass::Execute()
{
    // Get config reference
    const CloudConfig& config = m_configParser->GetParsedConfig();

    // Skip rendering if disabled
    if (!config.enabled)
    {
        return;
    }

    BeginPass();

    // Get camera position
    Vec3 cameraPos = g_theGame->m_player->m_position;

    // Get cloud animation time (includes tickDelta and CLOUD_TIME_SCALE)
    // Apply speed multiplier from config
    float cloudTime = g_theGame->m_timeProvider->GetCloudTime() * config.speed;

    // World coordinates with cloud scrolling offset
    float worldX = cameraPos.x + cloudTime;
    float worldY = cameraPos.y + CLOUD_OFFSET;

    // Cell coordinates (12x12 cell size)
    int cellX = static_cast<int>(std::floor(worldX / 12.0f));
    int cellY = static_cast<int>(std::floor(worldY / 12.0f));

    // Calculate cloud layer bounds from config
    float cloudMinZ = config.GetMinZ();
    float cloudMaxZ = config.GetMaxZ();

    // Calculate ViewOrientation based on camera height relative to cloud layer
    ViewOrientation orientation;
    if (m_renderMode == CloudStatus::FANCY)
    {
        orientation = ViewOrientationHelper::GetOrientation(cameraPos, cloudMinZ, cloudMaxZ);
    }
    else
    {
        orientation = ViewOrientation::BELOW_CLOUDS;
    }

    // Construct geometry parameters (use renderDistance from config)
    CloudGeometryParameters params(
        cellX, cellY, config.renderDistance,
        orientation, m_renderMode
    );

    // Rebuild geometry if parameters changed
    if (m_needsRebuild || params != m_cachedParams)
    {
        CloudGeometryHelper::RebuildGeometry(
            m_geometry.get(), params, m_textureData.get()
        );

        m_cachedParams = params;
        m_needsRebuild = false;
    }

    // Render cloud mesh
    if (m_geometry && !m_geometry->vertices.empty())
    {
        // [IMPORTANT] ModelMatrix Calculation
        // Geometry is in LOCAL space [-radius*12, +radius*12], must translate to WORLD space.
        // ModelMatrix positions geometry center at camera XY (with sub-cell offset) at cloud height.
        // After gbufferModelView transform: Final = (cameraPos - subCell) - cameraPos = -subCell
        // This matches Sodium's translate(-viewPosX, -viewPosY, -viewPosZ) approach.
        float subCellX = worldX - cellX * 12.0f;
        float subCellY = worldY - cellY * 12.0f;

        float translateX = cameraPos.x - subCellX;
        float translateY = cameraPos.y - subCellY;
        float translateZ = config.height; // Use height from config

        Mat44 modelMatrix = Mat44::MakeTranslation3D(Vec3(translateX, translateY, translateZ));

        // Calculate cloud color based on time of day
        Vec3 cloudColor = g_theGame->m_timeProvider->CalculateCloudColor(0.0f, 0.0f);

        // Upload uniforms
        PerObjectUniforms perObjectUniform;
        perObjectUniform.modelMatrix        = modelMatrix;
        perObjectUniform.modelMatrixInverse = modelMatrix.GetInverse();
        perObjectUniform.modelColor[0]      = cloudColor.x;
        perObjectUniform.modelColor[1]      = cloudColor.y;
        perObjectUniform.modelColor[2]      = cloudColor.z;
        perObjectUniform.modelColor[3]      = config.opacity; // Use opacity from config
        g_theRendererSubsystem->GetUniformManager()->UploadBuffer(perObjectUniform);

        // Render
        g_theRendererSubsystem->SetBlendConfig(BlendConfig::Alpha());
        // [REFACTOR] Pair-based RT binding
        g_theRendererSubsystem->UseProgram(m_cloudsShader, {{RTType::ColorTex, 0}, {RTType::ColorTex, 3}, {RTType::DepthTex, 0}});
        g_theRendererSubsystem->DrawVertexArray(m_geometry->vertices);
    }

    EndPass();
}

void CloudRenderPass::OnShaderBundleLoaded(enigma::graphic::ShaderBundle* newBundle)
{
    if (newBundle)
    {
        m_cloudsShader = newBundle->GetProgram("gbuffers_clouds");
    }
}

void CloudRenderPass::OnShaderBundleUnloaded()
{
    m_cloudsShader = nullptr;
}

/// Setup render states for cloud rendering
/// Uses GL_LESS depth (not LEQUAL) to prevent Z-fighting on semi-transparent faces
void CloudRenderPass::BeginPass()
{
    g_theRendererSubsystem->SetDepthConfig(DepthConfig::ReadOnly());
    g_theRendererSubsystem->SetBlendConfig(BlendConfig::Alpha());
    g_theRendererSubsystem->SetRasterizationConfig(RasterizationConfig::CullBack());
    g_theRendererSubsystem->SetVertexLayout(Vertex_PCUTBNLayout::Get());

    // Save the near and far for cloud
    auto camera  = g_theGame->m_player->GetCamera();
    m_cachedFar  = camera->GetFarPlane();
    m_cachedNear = camera->GetNearPlane();
    camera->SetNearFar(0.01f, 1000.f);
    auto matrixUniform = camera->GetMatrixUniforms();
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(matrixUniform);
}

/// Restore default render states
void CloudRenderPass::EndPass()
{
    g_theRendererSubsystem->SetDepthConfig(DepthConfig::Enabled());
    g_theRendererSubsystem->SetStencilTest(StencilTestDetail::Disabled());
    g_theRendererSubsystem->SetBlendConfig(BlendConfig::Opaque());

    // Restore the near and far
    auto camera = g_theGame->m_player->GetCamera();
    camera->SetNearFar(m_cachedNear, m_cachedFar);
    auto matrixUniform = camera->GetMatrixUniforms();
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(matrixUniform);
}

/// Load clouds.png and create CloudTextureData for CPU-side geometry generation
void CloudRenderPass::LoadCloudTexture()
{
    Image image(".enigma/assets/engine/textures/environment/clouds.png");

    if (image.GetDimensions() == IntVec2(0, 0))
    {
        return;
    }

    m_textureData = CloudTextureData::Load(image);

    if (!m_textureData)
    {
        return;
    }

    m_needsRebuild = true;
}

/// Set Fast/Fancy rendering mode (triggers geometry rebuild if changed)
void CloudRenderPass::SetRenderMode(CloudStatus mode)
{
    if (m_renderMode != mode)
    {
        m_renderMode   = mode;
        m_needsRebuild = true;
    }
}
