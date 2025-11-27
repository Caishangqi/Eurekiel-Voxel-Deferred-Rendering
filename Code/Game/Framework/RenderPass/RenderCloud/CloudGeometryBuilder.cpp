/**
 * @file CloudGeometryBuilder.cpp
 * @brief Cloud Mesh Generator Implementation (Minecraft Vanilla Style)
 * @date 2025-11-25
 */

#include "CloudGeometryBuilder.hpp"
#include "Engine/Math/MathUtils.hpp"

// [CONSTANTS] Cloud Mesh Generation Parameters
constexpr float FAST_CELL_SIZE        = 8.0f; // Fast mode: 8x8 blocks per cell
constexpr float FANCY_CELL_SIZE       = 12.0f; // Fancy mode: 12x12 blocks per cell
constexpr float FANCY_CLOUD_THICKNESS = 4.0f; // Fancy mode: 4 blocks thick

// [CONSTANTS] Texture Mapping (256x256 clouds.png)
constexpr float UV_SCALE = 1.0f / 256.0f; // 1 block = 1 pixel in 256x256 texture

/**
 * @brief Generate cloud mesh grid (Main Entry Point)
 */
std::vector<Vertex> CloudGeometryBuilder::GenerateCloudMesh(
    int          cellCount,
    float        cloudHeight,
    bool         fancyMode,
    const Rgba8& color)
{
    // [STEP 1] Calculate Total Vertex Count
    int verticesPerCell = fancyMode ? 24 : 6; // Fancy: 8 tri * 3 verts, Fast: 2 tri * 3 verts
    int totalVertices   = cellCount * cellCount * verticesPerCell;

    std::vector<Vertex> vertices;
    vertices.reserve(totalVertices);

    // [STEP 2] Generate Each Cell
    for (int cellY = 0; cellY < cellCount; ++cellY)
    {
        for (int cellX = 0; cellX < cellCount; ++cellX)
        {
            // Generate cell vertices based on mode
            std::vector<Vertex> cellVertices = fancyMode
                                                   ? GenerateFancyCell(cellX, cellY, cloudHeight, color)
                                                   : GenerateFastCell(cellX, cellY, cloudHeight, color);

            // Append to main vertex buffer
            vertices.insert(vertices.end(), cellVertices.begin(), cellVertices.end());
        }
    }

    return vertices;
}

/**
 * @brief Generate Fast Mode cell (2 triangles, 6 vertices)
 */
std::vector<Vertex> CloudGeometryBuilder::GenerateFastCell(
    int          cellX,
    int          cellY,
    float        cloudHeight,
    const Rgba8& color)
{
    std::vector<Vertex> vertices;
    vertices.reserve(6); // 2 triangles * 3 vertices

    // [STEP 1] Calculate Cell World Position (Engine Coordinate System: +Z up)
    // Minecraft (MC_X, MC_Y, MC_Z) → Engine (MC_X, MC_Z, MC_Y)
    float worldX = static_cast<float>(cellX) * FAST_CELL_SIZE;
    float worldY = static_cast<float>(cellY) * FAST_CELL_SIZE;
    float worldZ = cloudHeight;

    // [STEP 2] Calculate Cell Corners (8x8 block cell)
    Vec3 corner0(worldX, worldY, worldZ); // Bottom-left
    Vec3 corner1(worldX + FAST_CELL_SIZE, worldY, worldZ); // Bottom-right
    Vec3 corner2(worldX + FAST_CELL_SIZE, worldY + FAST_CELL_SIZE, worldZ); // Top-right
    Vec3 corner3(worldX, worldY + FAST_CELL_SIZE, worldZ); // Top-left

    // [STEP 3] Calculate UV Coordinates (256x256 texture, wraps across grid)
    float u0 = worldX * UV_SCALE;
    float v0 = worldY * UV_SCALE;
    float u1 = (worldX + FAST_CELL_SIZE) * UV_SCALE;
    float v1 = (worldY + FAST_CELL_SIZE) * UV_SCALE;

    // [STEP 4] Generate Top Face (2 triangles, visible from above)
    Vec3 normalUp(0.0f, 0.0f, 1.0f); // +Z up

    // Triangle 1: (0, 1, 2)
    vertices.push_back(Vertex(corner0, color, Vec2(u0, v0), normalUp));
    vertices.push_back(Vertex(corner1, color, Vec2(u1, v0), normalUp));
    vertices.push_back(Vertex(corner2, color, Vec2(u1, v1), normalUp));

    // Triangle 2: (0, 2, 3)
    vertices.push_back(Vertex(corner0, color, Vec2(u0, v0), normalUp));
    vertices.push_back(Vertex(corner2, color, Vec2(u1, v1), normalUp));
    vertices.push_back(Vertex(corner3, color, Vec2(u0, v1), normalUp));

    return vertices;
}

/**
 * @brief Generate Fancy Mode cell (8 triangles, 24 vertices)
 */
std::vector<Vertex> CloudGeometryBuilder::GenerateFancyCell(
    int          cellX,
    int          cellY,
    float        cloudHeight,
    const Rgba8& color)
{
    std::vector<Vertex> vertices;
    vertices.reserve(24); // 8 triangles * 3 vertices

    // [STEP 1] Calculate Cell World Position (Engine Coordinate System: +Z up)
    float worldX       = static_cast<float>(cellX) * FANCY_CELL_SIZE;
    float worldY       = static_cast<float>(cellY) * FANCY_CELL_SIZE;
    float worldZBottom = cloudHeight;
    float worldZTop    = cloudHeight + FANCY_CLOUD_THICKNESS;

    // [STEP 2] Calculate 8 Corners of Volumetric Cloud Block (12x12x4)
    // Bottom face (Z = cloudHeight)
    Vec3 corner0(worldX, worldY, worldZBottom); // Bottom-left-bottom
    Vec3 corner1(worldX + FANCY_CELL_SIZE, worldY, worldZBottom); // Bottom-right-bottom
    Vec3 corner2(worldX + FANCY_CELL_SIZE, worldY + FANCY_CELL_SIZE, worldZBottom); // Top-right-bottom
    Vec3 corner3(worldX, worldY + FANCY_CELL_SIZE, worldZBottom); // Top-left-bottom

    // Top face (Z = cloudHeight + 4)
    Vec3 corner4(worldX, worldY, worldZTop); // Bottom-left-top
    Vec3 corner5(worldX + FANCY_CELL_SIZE, worldY, worldZTop); // Bottom-right-top
    Vec3 corner6(worldX + FANCY_CELL_SIZE, worldY + FANCY_CELL_SIZE, worldZTop); // Top-right-top
    Vec3 corner7(worldX, worldY + FANCY_CELL_SIZE, worldZTop); // Top-left-top

    // [STEP 3] Calculate UV Coordinates (256x256 texture)
    float u0 = worldX * UV_SCALE;
    float v0 = worldY * UV_SCALE;
    float u1 = (worldX + FANCY_CELL_SIZE) * UV_SCALE;
    float v1 = (worldY + FANCY_CELL_SIZE) * UV_SCALE;

    // [STEP 4] Generate 6 Faces (8 triangles total)

    // Face 1: Top Face (+Z, 2 triangles)
    Vec3 normalUp(0.0f, 0.0f, 1.0f);
    vertices.push_back(Vertex(corner4, color, Vec2(u0, v0), normalUp));
    vertices.push_back(Vertex(corner5, color, Vec2(u1, v0), normalUp));
    vertices.push_back(Vertex(corner6, color, Vec2(u1, v1), normalUp));

    vertices.push_back(Vertex(corner4, color, Vec2(u0, v0), normalUp));
    vertices.push_back(Vertex(corner6, color, Vec2(u1, v1), normalUp));
    vertices.push_back(Vertex(corner7, color, Vec2(u0, v1), normalUp));

    // Face 2: Bottom Face (-Z, 2 triangles)
    Vec3 normalDown(0.0f, 0.0f, -1.0f);
    vertices.push_back(Vertex(corner0, color, Vec2(u0, v0), normalDown));
    vertices.push_back(Vertex(corner2, color, Vec2(u1, v1), normalDown));
    vertices.push_back(Vertex(corner1, color, Vec2(u1, v0), normalDown));

    vertices.push_back(Vertex(corner0, color, Vec2(u0, v0), normalDown));
    vertices.push_back(Vertex(corner3, color, Vec2(u0, v1), normalDown));
    vertices.push_back(Vertex(corner2, color, Vec2(u1, v1), normalDown));

    // Face 3: Front Face (+Y, 1 triangle)
    Vec3 normalFront(0.0f, 1.0f, 0.0f);
    vertices.push_back(Vertex(corner3, color, Vec2(u0, v0), normalFront));
    vertices.push_back(Vertex(corner7, color, Vec2(u0, v1), normalFront));
    vertices.push_back(Vertex(corner6, color, Vec2(u1, v1), normalFront));

    // Face 4: Back Face (-Y, 1 triangle)
    Vec3 normalBack(0.0f, -1.0f, 0.0f);
    vertices.push_back(Vertex(corner0, color, Vec2(u0, v0), normalBack));
    vertices.push_back(Vertex(corner1, color, Vec2(u1, v0), normalBack));
    vertices.push_back(Vertex(corner5, color, Vec2(u1, v1), normalBack));

    // Face 5: Right Face (+X, 1 triangle)
    Vec3 normalRight(1.0f, 0.0f, 0.0f);
    vertices.push_back(Vertex(corner1, color, Vec2(u0, v0), normalRight));
    vertices.push_back(Vertex(corner2, color, Vec2(u1, v0), normalRight));
    vertices.push_back(Vertex(corner6, color, Vec2(u1, v1), normalRight));

    // Face 6: Left Face (-X, 1 triangle)
    Vec3 normalLeft(-1.0f, 0.0f, 0.0f);
    vertices.push_back(Vertex(corner0, color, Vec2(u0, v0), normalLeft));
    vertices.push_back(Vertex(corner4, color, Vec2(u0, v1), normalLeft));
    vertices.push_back(Vertex(corner7, color, Vec2(u1, v1), normalLeft));

    return vertices;
}
