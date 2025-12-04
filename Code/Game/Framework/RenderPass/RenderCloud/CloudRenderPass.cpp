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

// ========================================
// Constants
// ========================================

/// Cloud layer height range (Engine Z-axis)
constexpr float CLOUD_HEIGHT    = 20.0f; // Base height
constexpr float CLOUD_THICKNESS = 4.0f; // Layer thickness
constexpr float CLOUD_MIN_Z     = CLOUD_HEIGHT;
constexpr float CLOUD_MAX_Z     = CLOUD_HEIGHT + CLOUD_THICKNESS;

/// Cloud animation offset (Sodium uses 0.33 for Z-axis offset)
constexpr float CLOUD_OFFSET = 0.33f;

/// Render distance: 16 cells = 192 blocks radius
constexpr int DEFAULT_RENDER_DISTANCE = 16;

CloudRenderPass::CloudRenderPass()
    : m_renderMode(CloudStatus::FANCY)
      , m_needsRebuild(true)
{
    // Load gbuffers_clouds shader
    enigma::graphic::ShaderCompileOptions shaderCompileOptions;
    shaderCompileOptions.enableDebugInfo = true;

    m_cloudsShader = g_theRendererSubsystem->CreateShaderProgramFromFiles(
        ".enigma/assets/engine/shaders/program/gbuffers_clouds.vs.hlsl",
        ".enigma/assets/engine/shaders/program/gbuffers_clouds.ps.hlsl",
        "gbuffers_clouds",
        shaderCompileOptions
    );

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
    BeginPass();

    // Get camera position
    Vec3 cameraPos = g_theGame->m_player->m_position;

    // Get cloud animation time (includes tickDelta and CLOUD_TIME_SCALE)
    float cloudTime = g_theGame->m_timeOfDayManager->GetCloudTime();

    // World coordinates with cloud scrolling offset
    float worldX = cameraPos.x + cloudTime;
    float worldY = cameraPos.y + CLOUD_OFFSET;

    // Cell coordinates (12x12 cell size)
    int cellX = static_cast<int>(std::floor(worldX / 12.0f));
    int cellY = static_cast<int>(std::floor(worldY / 12.0f));

    // Calculate ViewOrientation based on camera height relative to cloud layer
    ViewOrientation orientation;
    if (m_renderMode == CloudStatus::FANCY)
    {
        orientation = ViewOrientationHelper::GetOrientation(
            cameraPos, CLOUD_MIN_Z, CLOUD_MAX_Z
        );
    }
    else
    {
        orientation = ViewOrientation::BELOW_CLOUDS;
    }

    // Construct geometry parameters
    CloudGeometryParameters params(
        cellX, cellY, DEFAULT_RENDER_DISTANCE,
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
        // Geometry is in LOCAL space [-192, +192], must translate to WORLD space.
        // ModelMatrix positions geometry center at camera XY (with sub-cell offset) at cloud height.
        // After gbufferModelView transform: Final = (cameraPos - subCell) - cameraPos = -subCell
        // This matches Sodium's translate(-viewPosX, -viewPosY, -viewPosZ) approach.
        float subCellX = worldX - cellX * 12.0f;
        float subCellY = worldY - cellY * 12.0f;

        float translateX = cameraPos.x - subCellX;
        float translateY = cameraPos.y - subCellY;
        float translateZ = CLOUD_HEIGHT;

        Mat44 modelMatrix = Mat44::MakeTranslation3D(Vec3(translateX, translateY, translateZ));

        // Calculate cloud color based on time of day
        Vec3 cloudColor = g_theGame->m_timeOfDayManager->CalculateCloudColor(0.0f, 0.0f);

        // Upload uniforms
        PerObjectUniforms perObjectUniform;
        perObjectUniform.modelMatrix        = modelMatrix;
        perObjectUniform.modelMatrixInverse = modelMatrix.GetInverse();
        perObjectUniform.modelColor[0]      = cloudColor.x;
        perObjectUniform.modelColor[1]      = cloudColor.y;
        perObjectUniform.modelColor[2]      = cloudColor.z;
        perObjectUniform.modelColor[3]      = 0.8f; // Sodium uses 0.8 alpha
        g_theRendererSubsystem->GetUniformManager()->UploadBuffer(perObjectUniform);

        // Render
        g_theRendererSubsystem->SetBlendMode(BlendMode::Alpha);
        std::vector<uint32_t> rtOutputs     = {0};
        int                   depthTexIndex = 0;
        g_theRendererSubsystem->UseProgram(m_cloudsShader, rtOutputs, depthTexIndex);
        g_theRendererSubsystem->DrawVertexArray(m_geometry->vertices);
    }

    EndPass();
}

/// Setup render states for cloud rendering
/// Uses GL_LESS depth (not LEQUAL) to prevent Z-fighting on semi-transparent faces
void CloudRenderPass::BeginPass()
{
    g_theRendererSubsystem->SetDepthMode(DepthMode::Less);
    g_theRendererSubsystem->SetBlendMode(BlendMode::Alpha);
    g_theRendererSubsystem->SetRasterizationConfig(RasterizationConfig::CullBack());
}

/// Restore default render states
void CloudRenderPass::EndPass()
{
    g_theRendererSubsystem->SetDepthMode(DepthMode::Enabled);
    g_theRendererSubsystem->SetStencilTest(StencilTestDetail::Disabled());
    g_theRendererSubsystem->SetBlendMode(BlendMode::Opaque);
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
