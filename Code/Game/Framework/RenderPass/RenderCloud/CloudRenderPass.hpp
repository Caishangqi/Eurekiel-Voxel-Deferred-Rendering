/**
 * @file CloudRenderPass.hpp
 * @brief [REWRITE] Cloud Rendering Pass (Sodium-style Vanilla Clouds)
 * @date 2025-12-02
 *
 * Architecture:
 * - Sodium CloudRenderer.java architecture (Line 70-528)
 * - CPU-side texture sampling (CloudTextureData)
 * - CPU-side geometry generation (CloudGeometryHelper)
 * - Intelligent face culling (no Z-Fighting)
 * - Fast/Fancy dual modes
 * - Spiral traversal algorithm (center-to-outer expansion)
 *
 * Features:
 * - Cloud height: Z=192-196 (4-block thickness)
 * - Fast Mode: Single horizontal face per cell
 * - Fancy Mode: Full 6-face volumetric cells + interior faces
 * - Alpha blending for semi-transparent clouds
 * - Rebuild only on parameter changes (camera move, mode switch)
 *
 * Coordinate System Mapping (Minecraft -> Engine):
 * - Minecraft +X (East) -> Engine +Y (Left)
 * - Minecraft +Y (Up) -> Engine +Z (Up)
 * - Minecraft +Z (South) -> Engine +X (Forward)
 *
 * Reference:
 * - Sodium CloudRenderer.java: F:\p4\Personal\SD\Thesis\ReferenceProject\Sodium\...
 * - Design Doc: .spec-workflow\specs\deferred-rendering-production-cloud-vanilla\design.md
 */

#pragma once
#include <memory>
#include <vector>

#include "Engine/Graphic/Core/EnigmaGraphicCommon.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Game/Framework/RenderPass/SceneRenderPass.hpp"

// Forward declarations
namespace enigma::graphic
{
    class ShaderProgram;
}

class CloudTextureData;
struct CloudGeometry;
struct CloudGeometryParameters;
enum class CloudStatus;
enum class ViewOrientation;

// ========================================
// Supporting Data Structures
// ========================================

/**
 * @enum CloudStatus
 * @brief Cloud rendering quality mode
 */
enum class CloudStatus
{
    FAST, // [NEW] Single flat face per cell (~32K vertices)
    FANCY // [NEW] Full volumetric cells (~98K vertices)
};

/**
 * @enum ViewOrientation
 * @brief Camera position relative to cloud layer
 *
 * Reference: Sodium CloudRenderer.java Line 696-714
 */
enum class ViewOrientation
{
    BELOW_CLOUDS, // [NEW] Camera below cloud layer (Z < 192)
    INSIDE_CLOUDS, // [NEW] Camera inside cloud layer (192 <= Z <= 196)
    ABOVE_CLOUDS // [NEW] Camera above cloud layer (Z > 196)
};

/**
 * @struct CloudGeometryParameters
 * @brief Parameters that determine geometry generation
 *
 * Geometry is rebuilt only when these parameters change.
 * 
 * Coordinate System (matching Sodium):
 * - originX: Player's cell X coordinate (Engine X axis, with cloud drift)
 * - originY: Player's cell Y coordinate (Engine Y axis)
 * Reference: Sodium CloudRenderer.java Line 716-723
 */
struct CloudGeometryParameters
{
    int             originX; // [FIX] Player cell X (Engine X axis)
    int             originY; // [FIX] Player cell Y (Engine Y axis)
    int             radius; // [NEW] Render distance in cells
    ViewOrientation orientation; // [NEW] Camera direction
    CloudStatus     renderMode; // [NEW] Fast/Fancy mode

    // [NEW] Default constructor
    CloudGeometryParameters()
        : originX(0), originY(0), radius(0)
          , orientation(ViewOrientation::BELOW_CLOUDS)
          , renderMode(CloudStatus::FAST)
    {
    }

    // [NEW] Parameterized constructor
    // [FIX] Parameter order: (x, y, radius, orientation, mode) - matching Sodium
    CloudGeometryParameters(int x, int y, int r, ViewOrientation o, CloudStatus m)
        : originX(x), originY(y), radius(r), orientation(o), renderMode(m)
    {
    }

    // [NEW] Equality operator for change detection
    bool operator==(const CloudGeometryParameters& other) const
    {
        return originX == other.originX &&
            originY == other.originY &&
            radius == other.radius &&
            orientation == other.orientation &&
            renderMode == other.renderMode;
    }

    bool operator!=(const CloudGeometryParameters& other) const
    {
        return !(*this == other);
    }
};

/**
 * @struct CloudGeometry
 * @brief Cached cloud geometry data
 *
 * Stores generated vertices and the parameters used to generate them.
 * [FIX 5] Lightweight data structure (no inheritance from Geometry)
 */
struct CloudGeometry
{
    std::vector<Vertex>     vertices; // [NEW] CPU-side vertex buffer
    CloudGeometryParameters params; // [NEW] Generation parameters

    CloudGeometry()  = default;
    ~CloudGeometry() = default;
};

/**
 * @namespace ViewOrientationHelper
 * @brief Helper functions for ViewOrientation calculations
 */
namespace ViewOrientationHelper
{
    /**
     * @brief Calculate ViewOrientation based on camera position
     * @param cameraPos Camera position in world space
     * @param cloudMinZ Cloud layer minimum Z (192.0f)
     * @param cloudMaxZ Cloud layer maximum Z (196.0f)
     * @return ViewOrientation enum value
     *
     * Reference: Sodium CloudRenderer.java Line 696-714
     * [IMPORTANT] Coordinate system mapped: Minecraft Y -> Engine Z
     */
    inline ViewOrientation GetOrientation(
        const Vec3& cameraPos,
        float       cloudMinZ,
        float       cloudMaxZ
    )
    {
        if (cameraPos.z <= cloudMinZ + 0.125f)
        {
            return ViewOrientation::BELOW_CLOUDS;
        }
        else if (cameraPos.z >= cloudMaxZ - 0.125f)
        {
            return ViewOrientation::ABOVE_CLOUDS;
        }
        else
        {
            return ViewOrientation::INSIDE_CLOUDS;
        }
    }
}


/**
 * @class CloudRenderPass
 * @brief [REWRITE] Manages cloud rendering lifecycle and owns cloud resources
 *
 * Responsibilities:
 * 1. Owns CloudTextureData (loaded from clouds.png)
 * 2. Owns CloudGeometry (cached vertex buffer)
 * 3. Coordinates CloudGeometryHelper for geometry generation
 * 4. Manages rendering states and shader binding
 * 5. Detects parameter changes and triggers rebuild
 *
 * Render Flow:
 * 1. Execute(): Calculate parameters -> Rebuild if needed -> Render
 * 2. BeginPass(): Set depth/blend/cull states
 * 3. EndPass(): Restore default states
 */
class CloudRenderPass : public SceneRenderPass
{
public:
    CloudRenderPass();
    ~CloudRenderPass() override;

public:
    // [REQUIRED] SceneRenderPass interface implementation
    void Execute() override;

protected:
    // [REQUIRED] Resource management hooks
    void BeginPass() override;
    void EndPass() override;

public:
    // [NEW] Public API for external control
    /**
     * @brief Load clouds.png texture and create CloudTextureData
     *
     * Called automatically in constructor.
     * Can be called manually to reload texture (e.g., resource pack change).
     */
    void LoadCloudTexture();

    /**
     * @brief Force rebuild cloud geometry on next frame
     *
     * Use case: User manually changes render distance or mode.
     */
    void RequestRebuild() { m_needsRebuild = true; }

    /**
     * @brief Get/Set Fast/Fancy rendering mode
     */
    CloudStatus GetRenderMode() const { return m_renderMode; }
    void        SetRenderMode(CloudStatus mode);

private:
    // [NEW] Core rendering logic
    /**
     * @brief Calculate cloud parameters and rebuild geometry if needed
     *
     * Parameters:
     * - Cell origin (cellY, cellX) from camera position
     * - ViewOrientation (BELOW/INSIDE/ABOVE clouds)
     * - Render distance (from game settings)
     *
     * Rebuilds geometry only if parameters changed.
     */
    void RebuildIfNeeded();

private:
    // ========================================
    // Data Members
    // ========================================

    // [NEW] Cloud texture data (face masks + colors)
    std::unique_ptr<CloudTextureData> m_textureData;

    // [NEW] Cloud geometry (cached vertices + parameters)
    std::unique_ptr<CloudGeometry> m_geometry;

    // [REWRITE] Shader program (gbuffers_clouds)
    // [FIX 2] Use raw pointer, matching SkyRenderPass.hpp pattern
    std::shared_ptr<enigma::graphic::ShaderProgram> m_cloudsShader = nullptr;

    // [NEW] Cached geometry parameters (for rebuild detection)
    CloudGeometryParameters m_cachedParams;

    // [NEW] Rebuild flag (set by texture load or parameter change)
    bool m_needsRebuild = true;

    // [NEW] Rendering mode (FAST / FANCY)
    CloudStatus m_renderMode;
};
