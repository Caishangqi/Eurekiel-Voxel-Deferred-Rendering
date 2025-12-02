/**
 * @file CloudRenderPass.cpp
 * @brief [REWRITE] Cloud Rendering Pass Implementation (Sodium-style)
 * @date 2025-12-02
 *
 * Implementation Notes:
 * - Follows Sodium CloudRenderer.java architecture
 * - Uses CloudTextureData for CPU-side texture sampling
 * - Uses CloudGeometryHelper for geometry generation
 * - Coordinate system mapping: Minecraft(X,Y,Z) -> Engine(Y,Z,X)
 * - Cloud height range: Z=192.0f to Z=196.0f (4-block thickness)
 */

#include "CloudRenderPass.hpp"
#include "CloudTextureData.hpp"
#include "CloudGeometryHelper.hpp"
#include "Engine/Core/Image.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Gameplay/Game.hpp"
#include "Game/Framework/GameObject/PlayerCharacter.hpp"
#include <cmath>

#include "Engine/Graphic/Shader/Uniform/PerObjectUniforms.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"

// ========================================
// Constants
// ========================================

// [NEW] Cloud height constants (Engine Z-axis)
constexpr float CLOUD_HEIGHT    = 192.33f; // [NEW] Base cloud height (Minecraft 192)
constexpr float CLOUD_THICKNESS = 4.0f; // [NEW] Cloud layer thickness
constexpr float CLOUD_MIN_Z     = CLOUD_HEIGHT; // [NEW] 192.0f
constexpr float CLOUD_MAX_Z     = CLOUD_HEIGHT + CLOUD_THICKNESS; // [NEW] 196.0f

// [NEW] Cloud animation constants
// [REMOVED] CLOUD_SCROLL_SPEED - now using TimeOfDayManager::GetCloudTime()
constexpr float CLOUD_OFFSET = 0.33f; // [NEW] Static offset for X-axis

// [NEW] Default render distance (in cells)
constexpr int DEFAULT_RENDER_DISTANCE = 16; // [NEW] 16 cells = 192 blocks

/**
 * @brief CloudRenderPass Constructor
 *
 * Initialization:
 * 1. Load gbuffers_clouds shader
 * 2. Load clouds.png texture and create CloudTextureData
 * 3. Initialize CloudGeometry (empty, will be built on first Execute)
 * 4. Set default render mode (FAST)
 *
 * Reference: Sodium CloudRenderer.java Line 70-145 (constructor equivalent)
 */
CloudRenderPass::CloudRenderPass()
    : m_renderMode(CloudStatus::FANCY)
      , m_needsRebuild(true)
{
    // [STEP 1] Load gbuffers_clouds Shader
    // [FIX 2] Use CreateShaderProgramFromFiles, matching SkyRenderPass.cpp:23-35
    enigma::graphic::ShaderCompileOptions shaderCompileOptions;
    shaderCompileOptions.enableDebugInfo = true;

    m_cloudsShader = g_theRendererSubsystem->CreateShaderProgramFromFiles(
        ".enigma/assets/engine/shaders/program/gbuffers_clouds.vs.hlsl",
        ".enigma/assets/engine/shaders/program/gbuffers_clouds.ps.hlsl",
        "gbuffers_clouds",
        shaderCompileOptions
    );


    // [STEP 2] Load clouds.png and create CloudTextureData
    LoadCloudTexture();

    // [STEP 3] Initialize empty CloudGeometry
    m_geometry = std::make_unique<CloudGeometry>();
}

CloudRenderPass::~CloudRenderPass()
{
    // Smart pointers auto-cleanup
}

/**
 * @brief Execute cloud rendering
 *
 * Main Rendering Loop:
 * 1. Calculate cloud layer parameters (cell origin, orientation)
 * 2. Check if geometry needs rebuild (parameters changed)
 * 3. Rebuild geometry if needed (CloudGeometryHelper::RebuildGeometry)
 * 4. Render cloud mesh with gbuffers_clouds shader
 *
 * Reference: Sodium CloudRenderer.java Line 70-145
 */
void CloudRenderPass::Execute()
{
    BeginPass();

    // [STEP 1] Calculate cloud layer parameters
    // Reference: Sodium CloudRenderer.java Line 74-87

    // Get camera position (Engine coordinate system: +X forward, +Y left, +Z up)
    Vec3 cameraPos = g_theGame->m_player->m_position;

    // Calculate cloud time (tick-based animation)
    // Reference: Sodium CloudRenderer.java Line 77
    // [FIX] Use TimeOfDayManager::GetCloudTime() - already includes tickDelta and CLOUD_TIME_SCALE
    float cloudTime = g_theGame->m_timeOfDayManager->GetCloudTime();

    // [IMPORTANT] Coordinate system mapping: Minecraft(X,Z) -> Engine(Y,X)
    // Minecraft: +X East, +Z South
    // Engine: +Y Left, +X Forward

    // World coordinates (with cloud scrolling offset)
    // Reference: Sodium CloudRenderer.java Line 79-80
    float worldY = cameraPos.y + cloudTime; // [NEW] Minecraft X -> Our Y
    float worldX = cameraPos.x + CLOUD_OFFSET; // [NEW] Minecraft Z -> Our X

    // Cell coordinates (12x12 cell size)
    // Reference: Sodium CloudRenderer.java Line 82-83
    int cellY = static_cast<int>(std::floor(worldY / 12.0f)); // [NEW] Minecraft cellX -> Our cellY
    int cellX = static_cast<int>(std::floor(worldX / 12.0f)); // [NEW] Minecraft cellZ -> Our cellX

    // [STEP 2] Calculate ViewOrientation
    // Reference: Sodium CloudRenderer.java Line 696-714
    ViewOrientation orientation;
    if (m_renderMode == CloudStatus::FANCY)
    {
        orientation = ViewOrientationHelper::GetOrientation(
            cameraPos, CLOUD_MIN_Z, CLOUD_MAX_Z
        );
    }
    else
    {
        // Fast mode always uses BELOW_CLOUDS (single face rendering)
        orientation = ViewOrientation::BELOW_CLOUDS;
    }

    // [STEP 3] Construct CloudGeometryParameters
    CloudGeometryParameters params(
        cellY, cellX, DEFAULT_RENDER_DISTANCE,
        orientation, m_renderMode
    );

    // [STEP 4] Rebuild geometry if parameters changed
    if (m_needsRebuild || params != m_cachedParams)
    {
        // Reference: Sodium CloudRenderer.java Line 147-216
        CloudGeometryHelper::RebuildGeometry(
            m_geometry.get(), params, m_textureData.get()
        );
        m_cachedParams = params;
        m_needsRebuild = false;
    }

    // [STEP 5] Render cloud mesh (if has vertices)
    if (m_geometry && !m_geometry->vertices.empty())
    {
        // Calculate view position offset (for smooth scrolling)
        // Reference: Sodium CloudRenderer.java Line 97-99
        float viewPosY = worldY - cellY * 12.0f;
        float viewPosX = worldX - cellX * 12.0f;
        float viewPosZ = cameraPos.z - CLOUD_HEIGHT;

        // Build model matrix (Translation only)
        // Translate cloud mesh to camera-relative position
        Mat44 modelMatrix = Mat44::MakeTranslation3D(Vec3(-viewPosY, -viewPosX, -viewPosZ));

        // Upload model matrix to PerObjectUniforms
        PerObjectUniforms perObjectUniform;
        perObjectUniform.modelMatrix        = modelMatrix;
        perObjectUniform.modelMatrixInverse = modelMatrix.GetInverse();
        g_theRendererSubsystem->GetUniformManager()->UploadBuffer(perObjectUniform);

        // [FIX 3] Set blend mode (Alpha blending for clouds)
        // Reference: design.md Line 196-200
        g_theRendererSubsystem->SetBlendMode(BlendMode::Alpha);

        // [FIX 2] Use shader program
        // Reference: SkyRenderPass.cpp Line 106, design.md Line 202-204
        std::vector<uint32_t> rtOutputs     = {0}; // colortex0
        int                   depthTexIndex = 0; // depthtex0
        g_theRendererSubsystem->UseProgram(m_cloudsShader, rtOutputs, depthTexIndex);

        // [FIX 1] Draw vertex array
        // Reference: SkyRenderPass.cpp Line 110, design.md Line 206-208
        g_theRendererSubsystem->DrawVertexArray(m_geometry->vertices);
    }

    EndPass();
}

/**
 * @brief Begin cloud rendering pass
 *
 * Setup Render States:
 * 1. Set depth mode: ReadOnly + LessEqual (test enabled, write disabled)
 * 2. Set blend mode: Alpha blending for semi-transparent clouds
 * 3. Set cull mode: None (Fast mode) or Default (Fancy mode)
 *
 * Reference: design.md Line 1030-1039
 */
void CloudRenderPass::BeginPass()
{
    // [FIX 3] Set depth state (ReadOnly + LessEqual)
    // Clouds render after sky, depth test enabled but no depth write
    // Reference: SkyRenderPass.cpp Line 165, design.md Line 1037-1038
    g_theRendererSubsystem->SetDepthMode(DepthMode::LessEqual);

    // [FIX 3] Enable alpha blending
    // Reference: design.md Line 196
    g_theRendererSubsystem->SetBlendMode(BlendMode::Multiply);

    // [FIX 3] Set cull mode based on render mode
    // Fast mode: Disable backface culling (single face rendering)
    // Fancy mode: Disable backface culling too (we generate both exterior and interior faces)
    // Reference: design.md Line 197-200
    // [FIX] Both modes now use NoCull - Fancy mode has interior faces for viewing from inside
    g_theRendererSubsystem->SetRasterizationConfig(RasterizationConfig::NoCull());
}

/**
 * @brief End cloud rendering pass
 *
 * Restore Default States:
 * 1. Reset depth mode to Enabled (ReadWrite + LessEqual)
 * 2. Reset blend mode to Opaque
 * 3. Reset stencil test to Disabled
 *
 * Reference: SkyRenderPass.cpp Line 174-181, design.md Line 1041-1046
 */
void CloudRenderPass::EndPass()
{
    // [FIX 3] Reset depth state
    g_theRendererSubsystem->SetDepthMode(DepthMode::Enabled);

    // [FIX 3] Reset stencil state
    g_theRendererSubsystem->SetStencilTest(StencilTestDetail::Disabled());

    // [FIX 3] Reset blend mode
    g_theRendererSubsystem->SetBlendMode(BlendMode::Opaque);
}

/**
 * @brief Load clouds.png texture and create CloudTextureData
 *
 * Steps:
 * 1. Load clouds.png using Image class (256x256 RGBA)
 * 2. Create CloudTextureData from Image (CPU-side processing)
 * 3. Set rebuild flag to regenerate geometry
 *
 * Reference: Sodium CloudRenderer.java Line 498-528, design.md Line 213-232
 */
void CloudRenderPass::LoadCloudTexture()
{
    // [STEP 1] Load clouds.png using Engine Image class
    // [FIX 7] Use Image constructor instead of NativeImage
    // Reference: design-api-corrections.md Line 11-27
    Image image(".enigma/assets/engine/textures/environment/clouds.png");

    // Check if image loaded successfully
    if (image.GetDimensions() == IntVec2(0, 0))
    {
        // TODO: Log error using Logger system
        // LOGGER.Error("Failed to load clouds.png");
        return;
    }

    // [STEP 2] Create CloudTextureData from Image
    // This performs CPU-side texture analysis:
    // - Extract face visibility masks (6-bit per pixel)
    // - Extract colors (ARGB format)
    // Reference: Sodium CloudRenderer.java Line 498-528
    m_textureData = CloudTextureData::Load(image);

    if (!m_textureData)
    {
        // TODO: Log error using Logger system
        // LOGGER.Error("Failed to parse clouds.png");
        return;
    }

    // [STEP 3] Mark geometry for rebuild
    m_needsRebuild = true;
}

/**
 * @brief Set Fast/Fancy rendering mode
 * @param mode New render mode (FAST or FANCY)
 *
 * Triggers geometry rebuild if mode changed.
 */
void CloudRenderPass::SetRenderMode(CloudStatus mode)
{
    if (m_renderMode != mode)
    {
        m_renderMode   = mode;
        m_needsRebuild = true;
    }
}
