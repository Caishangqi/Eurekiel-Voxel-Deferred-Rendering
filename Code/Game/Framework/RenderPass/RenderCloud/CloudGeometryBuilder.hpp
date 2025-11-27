/**
 * @file CloudGeometryBuilder.hpp
 * @brief Cloud Mesh Generator (Minecraft Vanilla Style)
 * @date 2025-11-25
 *
 * Features:
 * - Generates 32x32 cell cloud mesh grid
 * - Fast Mode: 8x8 blocks per cell, 2 triangles (6144 vertices total)
 * - Fancy Mode: 12x12x4 blocks per cell, 8 triangles (24576 vertices total)
 * - Cloud height: Z=192-196 (Engine coordinate system, +Z up)
 * - Texture: 256x256 clouds.png
 *
 * Technical Specs:
 * - Fast Mode: Each cell = 8x8 blocks, 2 triangles (top/bottom faces)
 *   - Total vertices: 32 * 32 * 2 * 3 = 6144
 * - Fancy Mode: Each cell = 12x12x4 blocks, 8 triangles (6 faces)
 *   - Total vertices: 32 * 32 * 8 * 3 = 24576
 * - Coordinate Conversion: Minecraft (X, Y, Z) → Engine (X, Z, Y)
 * - Texture Mapping: 256x256 clouds.png, UV wraps across grid
 *
 * Minecraft Reference:
 * - LevelRenderer.java:buildClouds() - Cloud mesh generation
 * - CloudStatus: FAST (simple) vs FANCY (volumetric)
 */

#pragma once

#include "Engine/Graphic/Core/EnigmaGraphicCommon.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Core/Rgba8.hpp"
#include <vector>

/**
 * @class CloudGeometryBuilder
 * @brief Static-only helper class for generating Minecraft-style cloud meshes
 *
 * Usage:
 * @code
 * // Fast Mode (6144 vertices)
 * auto fastCloud = CloudGeometryBuilder::GenerateCloudMesh(32, 192.0f, false);
 *
 * // Fancy Mode (24576 vertices)
 * auto fancyCloud = CloudGeometryBuilder::GenerateCloudMesh(32, 192.0f, true);
 * @endcode
 */
class CloudGeometryBuilder
{
public:
    // [REQUIRED] Static-only class - Delete all constructors
    CloudGeometryBuilder()                                       = delete;
    CloudGeometryBuilder(const CloudGeometryBuilder&)            = delete;
    CloudGeometryBuilder& operator=(const CloudGeometryBuilder&) = delete;

    /**
     * @brief Generate cloud mesh grid (Minecraft vanilla style)
     * @param cellCount Number of cells per side (default: 32, creates 32x32 grid)
     * @param cloudHeight Base cloud height in Engine Z coordinate (default: 192.0f)
     * @param fancyMode true = Fancy (8 tri/cell, 24576 verts), false = Fast (2 tri/cell, 6144 verts)
     * @param color Vertex color (default: WHITE)
     * @return Vector of vertices ready for VertexBuffer creation
     *
     * Fast Mode (fancyMode=false):
     * - Each cell: 8x8 blocks, 2 triangles (top/bottom faces only)
     * - Total vertices: cellCount^2 * 2 * 3 = 32*32*2*3 = 6144
     *
     * Fancy Mode (fancyMode=true):
     * - Each cell: 12x12x4 blocks, 8 triangles (6 faces: top/bottom/4 sides)
     * - Total vertices: cellCount^2 * 8 * 3 = 32*32*8*3 = 24576
     *
     * Cloud Height Range:
     * - Fast Mode: Z = cloudHeight (single layer at 192)
     * - Fancy Mode: Z = cloudHeight to cloudHeight+4 (192-196, 4 blocks thick)
     *
     * Texture Mapping:
     * - 256x256 clouds.png texture
     * - UV coordinates wrap across grid (0-32 range maps to 0-1 UV)
     */
    static std::vector<Vertex> GenerateCloudMesh(
        int          cellCount   = 32,
        float        cloudHeight = 192.0f,
        bool         fancyMode   = false,
        const Rgba8& color       = Rgba8::WHITE
    );

private:
    /**
     * @brief Generate Fast Mode cell (2 triangles, 6 vertices)
     * @param cellX Cell X index (0-31)
     * @param cellY Cell Y index (0-31)
     * @param cloudHeight Base cloud height
     * @param color Vertex color
     * @return 6 vertices (2 triangles: top face + bottom face)
     *
     * Fast Mode Cell Structure:
     * - 8x8 blocks per cell
     * - 2 triangles: Top face (visible from above) + Bottom face (visible from below)
     * - Single layer at cloudHeight
     */
    static std::vector<Vertex> GenerateFastCell(
        int          cellX,
        int          cellY,
        float        cloudHeight,
        const Rgba8& color
    );

    /**
     * @brief Generate Fancy Mode cell (8 triangles, 24 vertices)
     * @param cellX Cell X index (0-31)
     * @param cellY Cell Y index (0-31)
     * @param cloudHeight Base cloud height
     * @param color Vertex color
     * @return 24 vertices (8 triangles: 6 faces of volumetric cloud block)
     *
     * Fancy Mode Cell Structure:
     * - 12x12x4 blocks per cell (volumetric)
     * - 8 triangles: Top (2 tri) + Bottom (2 tri) + 4 Sides (1 tri each)
     * - Height range: cloudHeight to cloudHeight+4
     */
    static std::vector<Vertex> GenerateFancyCell(
        int          cellX,
        int          cellY,
        float        cloudHeight,
        const Rgba8& color
    );
};
