/**
 * @file CloudGeometryHelper.hpp
 * @brief [NEW] Static helper for Sodium-style cloud geometry generation
 * @date 2025-12-02
 *
 * [REWRITE] CloudGeometryHelper - Static Tool Class
 * 
 * Features:
 * - Sodium spiral traversal algorithm (center-to-outer expansion)
 * - Face culling logic (GetVisibleFaces)
 * - Fast/Fancy geometry generation (EmitCellGeometry methods)
 * - CPU-side texture sampling (CloudTextureData integration)
 * - Brightness constants for directional shading
 *
 * Architecture:
 * - Pure static class (no state, no instances)
 * - Follows project Helper class conventions
 * - Directly manipulates std::vector<Vertex>
 *
 * Reference:
 * - Sodium CloudRenderer.java Line 147-444
 * - Design Doc: F:\p4\Personal\SD\Thesis\.spec-workflow\specs\deferred-rendering-production-cloud-vanilla\design.md
 */

#pragma once

#include "Engine/Graphic/Core/EnigmaGraphicCommon.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Core/Rgba8.hpp"
#include <vector>
#include <cstdint>

#include "CloudTextureData.hpp"

// Forward declarations
class CloudTextureData;
struct CloudGeometry;
struct CloudGeometryParameters;
enum class ViewOrientation;

/**
 * @class CloudGeometryHelper
 * @brief [NEW] Static-only tool class for cloud geometry generation
 *
 * Design Principles:
 * 1. No instances - All methods static
 * 2. Completely stateless - No static variables
 * 3. Single responsibility - Geometry generation only
 * 4. Naming convention - {Domain}Helper pattern
 *
 * Usage:
 * @code
 * CloudGeometryHelper::RebuildGeometry(geometryPtr, params, textureData);
 * @endcode
 */
class CloudGeometryHelper
{
public:
    // [REQUIRED] Delete all constructors (no instances allowed)
    CloudGeometryHelper()                                      = delete;
    ~CloudGeometryHelper()                                     = delete;
    CloudGeometryHelper(const CloudGeometryHelper&)            = delete;
    CloudGeometryHelper& operator=(const CloudGeometryHelper&) = delete;

    // ========================================
    // Core Methods
    // ========================================

    /**
     * @brief [NEW] Rebuild cloud geometry using spiral traversal algorithm
     * @param existingGeometry Geometry object to fill with vertices
     * @param params Generation parameters (origin, radius, orientation, mode)
     * @param textureData Cloud texture data for sampling
     *
     * Algorithm:
     * 1. Clear existing vertices and reserve ~50,000 capacity
     * 2. Create texture slice (wrap sampling)
     * 3. Process center cell (0, 0)
     * 4. Phase 1: Diamond expansion (layer 1 to radius)
     * 5. Phase 2: Corner filling (layer radius+1 to 2*radius)
     * 6. Shrink vertices to fit
     *
     * Reference: Sodium CloudRenderer.java Line 147-216
     */
    static void RebuildGeometry(
        CloudGeometry*                 existingGeometry,
        const CloudGeometryParameters& params,
        const CloudTextureData*        textureData
    );

    /**
     * @brief [NEW] Process single cell geometry
     * @param vertices Vertex vector to append to
     * @param slice Texture slice for sampling
     * @param x Cell X coordinate (relative to origin)
     * @param z Cell Z coordinate (relative to origin)
     * @param orientation Camera orientation
     * @param flat Fast mode flag (true = single face, false = full 6 faces)
     *
     * Logic:
     * 1. Get cell index and visible faces
     * 2. Apply face culling mask
     * 3. Skip if no visible faces or transparent
     * 4. Generate geometry based on mode (Flat vs Fancy)
     *
     * Reference: Sodium CloudRenderer.java Line 218-235
     */
    static void AddCellGeometry(
        std::vector<Vertex>&           vertices,
        const CloudTextureData::Slice& slice,
        int                            x, int z,
        ViewOrientation                orientation,
        bool                           flat
    );

    /**
     * @brief [NEW] Calculate visible faces mask for cell
     * @param x Cell X coordinate (relative to camera)
     * @param z Cell Z coordinate (relative to camera)
     * @param orientation Camera orientation (BELOW/INSIDE/ABOVE clouds)
     * @return 6-bit face mask (FACE_MASK_* constants)
     *
     * Logic:
     * - x <= 0: Render POS_Y face (camera on left)
     * - x >= 0: Render NEG_Y face (camera on right)
     * - z <= 0: Render POS_X face (camera behind)
     * - z >= 0: Render NEG_X face (camera in front)
     * - orientation != BELOW: Render POS_Z face (top)
     * - orientation != ABOVE: Render NEG_Z face (bottom)
     *
     * Reference: Sodium CloudRenderer.java Line 237-264
     */
    static int GetVisibleFaces(
        int             x, int z,
        ViewOrientation orientation
    );

    // ========================================
    // Geometry Generation Methods
    // ========================================

    /**
     * @brief [NEW] Generate Fast mode flat geometry (single face)
     * @param vertices Vertex vector to append to
     * @param color Cell color (ABGR32 format)
     * @param x Cell X coordinate
     * @param z Cell Z coordinate
     *
     * Generates:
     * - Single 12x12 horizontal face at Z=0
     * - 4 vertices (2 triangles)
     * - Top face brightness (BRIGHTNESS_POS_Z = 1.0f)
     *
     * Reference: Sodium CloudRenderer.java Line 266-297
     */
    static void EmitCellGeometryFlat(
        std::vector<Vertex>& vertices,
        uint32_t             color,
        int                  x, int z
    );

    /**
     * @brief [NEW] Generate Fancy mode exterior faces (6 faces)
     * @param vertices Vertex vector to append to
     * @param cellFaces Visible faces mask (6-bit)
     * @param cellColor Cell color (ABGR32 format)
     * @param cellX Cell X coordinate
     * @param cellZ Cell Z coordinate
     *
     * Generates:
     * - Up to 6 faces: NEG_Z, POS_Z, NEG_Y, POS_Y, NEG_X, POS_X
     * - 12x12x4 volumetric block
     * - 4 vertices per face (2 triangles)
     * - Directional brightness applied
     *
     * Reference: Sodium CloudRenderer.java Line 299-386
     */
    static void EmitCellGeometryExterior(
        std::vector<Vertex>& vertices,
        int                  cellFaces,
        uint32_t             cellColor,
        int                  cellX, int cellZ
    );

    /**
     * @brief [NEW] Generate Fancy mode interior faces (reversed normals)
     * @param vertices Vertex vector to append to
     * @param baseColor Base color (ABGR32 format)
     * @param cellX Cell X coordinate
     * @param cellZ Cell Z coordinate
     *
     * Generates:
     * - Fixed 6 faces (all faces, no culling)
     * - 12x12x4 volumetric block
     * - Vertices in reversed order (interior view)
     * - 24 vertices total (6 faces × 4 vertices)
     *
     * Reference: Sodium CloudRenderer.java Line 388-444
     */
    static void EmitCellGeometryInterior(
        std::vector<Vertex>& vertices,
        uint32_t             baseColor,
        int                  cellX, int cellZ
    );

    // ========================================
    // Helper Functions
    // ========================================

    /**
     * @brief [NEW] Calculate taxicab distance (Manhattan distance)
     * @param x X coordinate
     * @param z Z coordinate
     * @return Taxicab distance from origin
     */
    static inline int TaxicabDistance(int x, int z)
    {
        return std::abs(x) + std::abs(z);
    }

    /**
     * @brief [NEW] Check if color is transparent (alpha < 10)
     * @param argb Color in ARGB format
     * @return true if transparent, false otherwise
     */
    static inline bool IsTransparent(uint32_t argb)
    {
        return ((argb >> 24) & 0xFF) < 10;
    }

    /**
     * @brief [FIX] Unpack ARGB32 color to Rgba8
     * @param argb Color in ARGB32 format (matches CloudTextureData::PackARGB)
     * @return Rgba8 color
     *
     * Format: ARGB (32-bit) - 0xAARRGGBB
     * - A: bits 24-31
     * - R: bits 16-23
     * - G: bits 8-15
     * - B: bits 0-7
     *
     * [FIX] Changed from ABGR to ARGB to match CloudTextureData storage format
     */
    static inline Rgba8 UnpackARGB32(uint32_t argb)
    {
        unsigned char a = (argb >> 24) & 0xFF;
        unsigned char r = (argb >> 16) & 0xFF;
        unsigned char g = (argb >> 8) & 0xFF;
        unsigned char b = argb & 0xFF;
        return Rgba8(r, g, b, a);
    }

    /**
     * @brief [FIX] Multiply color brightness (RGB only, preserve alpha)
     * @param argb Color in ARGB32 format (matches CloudTextureData storage)
     * @param brightness Brightness multiplier (0.0-1.0)
     * @return Modified color in ARGB32 format
     *
     * Applies brightness to RGB channels:
     * - R' = R * brightness
     * - G' = G * brightness
     * - B' = B * brightness
     * - A' = A (unchanged)
     *
     * [FIX] Changed from ABGR to ARGB to match CloudTextureData storage format
     */
    static inline uint32_t MultiplyColorBrightness(uint32_t argb, float brightness)
    {
        unsigned char a = (argb >> 24) & 0xFF;
        unsigned char r = static_cast<unsigned char>(((argb >> 16) & 0xFF) * brightness);
        unsigned char g = static_cast<unsigned char>(((argb >> 8) & 0xFF) * brightness);
        unsigned char b = static_cast<unsigned char>((argb & 0xFF) * brightness);
        return (a << 24) | (r << 16) | (g << 8) | b;
    }
};

// ========================================
// Brightness Constants
// ========================================

/**
 * @brief [NEW] Directional brightness constants
 *
 * Coordinate System Mapping (Minecraft -> Engine):
 * - Minecraft POS_Y (up, 1.0f) -> Engine POS_Z (up)
 * - Minecraft NEG_Y (down, 0.7f) -> Engine NEG_Z (down)
 * - Minecraft X_AXIS (east-west, 0.9f) -> Engine Y_AXIS (left-right)
 * - Minecraft Z_AXIS (north-south, 0.8f) -> Engine X_AXIS (front-back)
 *
 * Reference: Sodium CloudRenderer.java Line 47-58
 */
constexpr float BRIGHTNESS_POS_Z  = 1.0f; // [NEW] Top face (upward)
constexpr float BRIGHTNESS_NEG_Z  = 0.7f; // [NEW] Bottom face (downward)
constexpr float BRIGHTNESS_Y_AXIS = 0.9f; // [NEW] Y-axis faces (left-right)
constexpr float BRIGHTNESS_X_AXIS = 0.8f; // [NEW] X-axis faces (front-back)
