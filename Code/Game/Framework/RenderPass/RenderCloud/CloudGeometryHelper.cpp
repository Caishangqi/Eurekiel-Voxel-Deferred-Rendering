/**
 * @file CloudGeometryHelper.cpp
 * @brief [NEW] Implementation of Sodium-style cloud geometry generation
 * @date 2025-12-02
 */

#include "CloudGeometryHelper.hpp"
#include "CloudTextureData.hpp"
#include "CloudRenderPass.hpp"  // [FIX] Include for CloudGeometry, CloudGeometryParameters, ViewOrientation
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Core/VertexUtils.hpp"  // [FIX] For AddVertsForQuad3D (generates triangles, not quads)
#include <algorithm>
#include <cmath>

// [FIX] Removed local struct/enum definitions - now using CloudRenderPass.hpp definitions

// ========================================
// Core Methods
// ========================================

/**
 * @brief [NEW] Rebuild cloud geometry using spiral traversal algorithm
 * Reference: Sodium CloudRenderer.java Line 147-216
 */
void CloudGeometryHelper::RebuildGeometry(
    CloudGeometry*                 existingGeometry,
    const CloudGeometryParameters& params,
    const CloudTextureData*        textureData
)
{
    // [STEP 1] Clear and reserve space
    existingGeometry->vertices.clear();
    existingGeometry->vertices.reserve(50000); // Pre-allocate estimated max vertices

    int radius = params.radius;
    // [FIX] Use actual params instead of hardcoded defaults
    ViewOrientation orientation = params.orientation;
    bool            flat        = (params.renderMode == CloudStatus::FAST);

    // [STEP 2] Get texture slice (wrap sampling)
    // Coordinate mapping: Minecraft (originX, originZ) -> Engine (originY, originX)
    CloudTextureData::Slice slice = textureData->CreateSlice(
        params.originY, params.originX, radius
    );

    // [STEP 3] Spiral traversal algorithm
    // Process center cell (0, 0)
    AddCellGeometry(existingGeometry->vertices, slice, 0, 0, orientation, flat);

    // [STEP 4] Phase 1: Diamond expansion (菱形向外扩展)
    // Reference: Sodium CloudRenderer.java Line 175-191
    for (int layer = 1; layer <= radius; ++layer)
    {
        // Edge 1: From bottom-left to top-right
        for (int l = -layer; l < layer; ++l)
        {
            int z = std::abs(l) - layer;
            AddCellGeometry(existingGeometry->vertices, slice, z, l, orientation, flat);
        }

        // Edge 2: From top-right to bottom-right
        for (int l = layer; l > -layer; --l)
        {
            int z = layer - std::abs(l);
            AddCellGeometry(existingGeometry->vertices, slice, z, l, orientation, flat);
        }
    }

    // [STEP 5] Phase 2: Corner filling (填充角落)
    // Reference: Sodium CloudRenderer.java Line 193-214
    for (int layer = radius + 1; layer <= 2 * radius; ++layer)
    {
        int l = layer - radius;

        // Corner 1: Bottom-left
        for (int z = -radius; z <= -l; ++z)
        {
            int x = -z - layer;
            AddCellGeometry(existingGeometry->vertices, slice, x, z, orientation, flat);
        }

        // Corner 2: Bottom-right
        for (int z = l; z <= radius; ++z)
        {
            int x = z - layer;
            AddCellGeometry(existingGeometry->vertices, slice, x, z, orientation, flat);
        }

        // Corner 3: Top-right
        for (int z = radius; z >= l; --z)
        {
            int x = layer - z;
            AddCellGeometry(existingGeometry->vertices, slice, x, z, orientation, flat);
        }

        // Corner 4: Top-left
        for (int z = -l; z >= -radius; --z)
        {
            int x = layer + z;
            AddCellGeometry(existingGeometry->vertices, slice, x, z, orientation, flat);
        }
    }

    // [STEP 6] Shrink to fit
    existingGeometry->vertices.shrink_to_fit();
}

/**
 * @brief [NEW] Process single cell geometry
 * Reference: Sodium CloudRenderer.java Line 218-235
 */
void CloudGeometryHelper::AddCellGeometry(
    std::vector<Vertex>&           vertices,
    const CloudTextureData::Slice& slice,
    int                            x, int z,
    ViewOrientation                orientation,
    bool                           flat
)
{
    // [STEP 1] Get cell index and visible faces
    int index     = slice.GetCellIndex(x, z);
    int cellFaces = slice.GetCellFaces(index) & GetVisibleFaces(x, z, orientation);

    // [STEP 2] Skip if no visible faces
    if (cellFaces == 0)
    {
        return;
    }

    // [STEP 3] Get color and check transparency
    uint32_t color = slice.GetCellColor(index);
    if (IsTransparent(color))
    {
        return;
    }

    // [STEP 4] Generate geometry based on mode
    if (flat)
    {
        // Fast mode: Single flat face
        EmitCellGeometryFlat(vertices, color, x, z);
    }
    else
    {
        // Fancy mode: Exterior faces
        EmitCellGeometryExterior(vertices, cellFaces, color, x, z);

        // Fancy mode: Interior faces (only for nearby cells)
        if (TaxicabDistance(x, z) <= 1)
        {
            EmitCellGeometryInterior(vertices, color, x, z);
        }
    }
}

/**
 * @brief [SIMPLIFIED] Calculate visible faces mask
 *
 * [FIX] 简化逻辑：不做基于相机位置的面剔除
 * 只依赖 GetOpenFaces 的邻居检测来剔除云体内部的面
 * 类似于 Minecraft 方块渲染：相邻方块之间的面不渲染，暴露面总是渲染
 *
 * 参数 x, z, orientation 保留但不使用，保持接口兼容
 */
int CloudGeometryHelper::GetVisibleFaces(
    int             x, int z,
    ViewOrientation orientation
)
{
    // [SIMPLIFIED] 返回所有6个面的掩码
    // 实际的面剔除由 GetOpenFaces 根据邻居是否透明来决定
    // 这样无论相机在云的上方还是下方，所有暴露的面都会渲染
    (void)x; // Unused
    (void)z; // Unused
    (void)orientation; // Unused

    return FACE_MASK_NEG_Z | FACE_MASK_POS_Z | // 底面 + 顶面
        FACE_MASK_NEG_Y | FACE_MASK_POS_Y | // 左面 + 右面
        FACE_MASK_NEG_X | FACE_MASK_POS_X; // 后面 + 前面
}

// ========================================
// Geometry Generation Methods
// ========================================

/**
 * @brief [NEW] Generate Fast mode flat geometry
 * Reference: Sodium CloudRenderer.java Line 266-297
 * [FIX] Use AddVertsForQuad3D to generate 6 vertices (2 triangles) instead of 4 vertices (1 quad)
 */
void CloudGeometryHelper::EmitCellGeometryFlat(
    std::vector<Vertex>& vertices,
    uint32_t             color,
    int                  x, int z
)
{
    // [STEP 1] Calculate cell position (12x12 horizontal plane)
    // [IMPORTANT] Coordinate system mapping: Minecraft(X,Y,Z) -> Ours(Y,Z,X)
    float y0 = x * 12.0f; // Minecraft x0 -> Our y0
    float y1 = y0 + 12.0f;
    float x0 = z * 12.0f; // Minecraft z0 -> Our x0
    float x1 = x0 + 12.0f;
    float z0 = 0.0f; // Minecraft y -> Our z (height fixed at 0)

    // [STEP 2] Calculate color (top face brightness 1.0)
    uint32_t vertexColor = MultiplyColorBrightness(color, BRIGHTNESS_POS_Z);
    Rgba8    rgba        = UnpackARGB32(vertexColor);

    // [STEP 3] Generate 6 vertices (2 triangles) using AddVertsForQuad3D
    // [FIX] Engine only supports TriangleList, not QuadList
    // AddVertsForQuad3D params: bottomLeft, bottomRight, topRight, topLeft
    AddVertsForQuad3D(vertices,
                      Vec3(y0, x0, z0), // bottomLeft
                      Vec3(y1, x0, z0), // bottomRight
                      Vec3(y1, x1, z0), // topRight
                      Vec3(y0, x1, z0), // topLeft
                      rgba
    );
}

/**
 * @brief [NEW] Generate Fancy mode exterior faces
 * Reference: Sodium CloudRenderer.java Line 299-386 (coordinate system mapped)
 * [FIX] Use AddVertsForQuad3D to generate 6 vertices (2 triangles) per face instead of 4 vertices
 */
void CloudGeometryHelper::EmitCellGeometryExterior(
    std::vector<Vertex>& vertices,
    int                  cellFaces,
    uint32_t             cellColor,
    int                  cellX, int cellZ
)
{
    // [IMPORTANT] Coordinate system mapping: Minecraft(X,Y,Z) -> Ours(Y,Z,X)
    // Cell dimensions: 12x12 (horizontal) x 4 (height)
    float y0 = cellX * 12.0f; // Minecraft x0 -> Our y0
    float y1 = y0 + 12.0f;
    float x0 = cellZ * 12.0f; // Minecraft z0 -> Our x0
    float x1 = x0 + 12.0f;
    float z0 = 0.0f; // Minecraft y0 -> Our z0
    float z1 = 4.0f; // Minecraft y1 -> Our z1

    // [FACE 1] Bottom face (Minecraft NEG_Y -> Our NEG_Z)
    // Looking from below, CCW winding
    if (cellFaces & FACE_MASK_NEG_Z)
    {
        uint32_t colorVal = MultiplyColorBrightness(cellColor, BRIGHTNESS_NEG_Z);
        Rgba8    rgba     = UnpackARGB32(colorVal);
        AddVertsForQuad3D(vertices,
                          Vec3(y0, x0, z0), // bottomLeft
                          Vec3(y0, x1, z0), // bottomRight
                          Vec3(y1, x1, z0), // topRight
                          Vec3(y1, x0, z0), // topLeft
                          rgba
        );
    }

    // [FACE 2] Top face (Minecraft POS_Y -> Our POS_Z)
    // Looking from above, CCW winding
    if (cellFaces & FACE_MASK_POS_Z)
    {
        uint32_t colorVal = MultiplyColorBrightness(cellColor, BRIGHTNESS_POS_Z);
        Rgba8    rgba     = UnpackARGB32(colorVal);
        AddVertsForQuad3D(vertices,
                          Vec3(y0, x0, z1), // bottomLeft
                          Vec3(y1, x0, z1), // bottomRight
                          Vec3(y1, x1, z1), // topRight
                          Vec3(y0, x1, z1), // topLeft
                          rgba
        );
    }

    // [FACE 3] Left face (Minecraft NEG_X -> Our NEG_Y)
    // Looking from -Y direction
    if (cellFaces & FACE_MASK_NEG_Y)
    {
        uint32_t colorVal = MultiplyColorBrightness(cellColor, BRIGHTNESS_Y_AXIS);
        Rgba8    rgba     = UnpackARGB32(colorVal);
        AddVertsForQuad3D(vertices,
                          Vec3(y0, x0, z0), // bottomLeft
                          Vec3(y0, x1, z0), // bottomRight
                          Vec3(y0, x1, z1), // topRight
                          Vec3(y0, x0, z1), // topLeft
                          rgba
        );
    }

    // [FACE 4] Right face (Minecraft POS_X -> Our POS_Y)
    // Looking from +Y direction
    if (cellFaces & FACE_MASK_POS_Y)
    {
        uint32_t colorVal = MultiplyColorBrightness(cellColor, BRIGHTNESS_Y_AXIS);
        Rgba8    rgba     = UnpackARGB32(colorVal);
        AddVertsForQuad3D(vertices,
                          Vec3(y1, x1, z0), // bottomLeft
                          Vec3(y1, x0, z0), // bottomRight
                          Vec3(y1, x0, z1), // topRight
                          Vec3(y1, x1, z1), // topLeft
                          rgba
        );
    }

    // [FACE 5] Back face (Minecraft NEG_Z -> Our NEG_X)
    // Looking from -X direction
    if (cellFaces & FACE_MASK_NEG_X)
    {
        uint32_t colorVal = MultiplyColorBrightness(cellColor, BRIGHTNESS_X_AXIS);
        Rgba8    rgba     = UnpackARGB32(colorVal);
        AddVertsForQuad3D(vertices,
                          Vec3(y1, x0, z0), // bottomLeft
                          Vec3(y0, x0, z0), // bottomRight
                          Vec3(y0, x0, z1), // topRight
                          Vec3(y1, x0, z1), // topLeft
                          rgba
        );
    }

    // [FACE 6] Front face (Minecraft POS_Z -> Our POS_X)
    // Looking from +X direction
    if (cellFaces & FACE_MASK_POS_X)
    {
        uint32_t colorVal = MultiplyColorBrightness(cellColor, BRIGHTNESS_X_AXIS);
        Rgba8    rgba     = UnpackARGB32(colorVal);
        AddVertsForQuad3D(vertices,
                          Vec3(y0, x1, z0), // bottomLeft
                          Vec3(y1, x1, z0), // bottomRight
                          Vec3(y1, x1, z1), // topRight
                          Vec3(y0, x1, z1), // topLeft
                          rgba
        );
    }
}

/**
 * @brief [NEW] Generate Fancy mode interior faces
 * Reference: Sodium CloudRenderer.java Line 388-444
 * [FIX] Use AddVertsForQuad3D to generate 6 vertices (2 triangles) per face instead of 4 vertices
 * Interior faces have reversed winding order compared to exterior faces
 */
void CloudGeometryHelper::EmitCellGeometryInterior(
    std::vector<Vertex>& vertices,
    uint32_t             baseColor,
    int                  cellX, int cellZ
)
{
    // [IMPORTANT] Interior faces = Exterior faces with reversed vertex winding
    // Fixed generation of 6 faces × 6 vertices = 36 vertices (2 triangles per face)

    // Calculate cell position (coordinate system mapping)
    float y0 = cellX * 12.0f;
    float y1 = y0 + 12.0f;
    float x0 = cellZ * 12.0f;
    float x1 = x0 + 12.0f;
    float z0 = 0.0f;
    float z1 = 4.0f;

    // [FACE 1] Bottom face (reversed winding - looking from inside)
    {
        uint32_t colorVal = MultiplyColorBrightness(baseColor, BRIGHTNESS_NEG_Z);
        Rgba8    rgba     = UnpackARGB32(colorVal);
        AddVertsForQuad3D(vertices,
                          Vec3(y1, x0, z0), // bottomLeft (reversed)
                          Vec3(y1, x1, z0), // bottomRight
                          Vec3(y0, x1, z0), // topRight
                          Vec3(y0, x0, z0), // topLeft
                          rgba
        );
    }

    // [FACE 2] Top face (reversed winding)
    {
        uint32_t colorVal = MultiplyColorBrightness(baseColor, BRIGHTNESS_POS_Z);
        Rgba8    rgba     = UnpackARGB32(colorVal);
        AddVertsForQuad3D(vertices,
                          Vec3(y1, x0, z1), // bottomLeft (reversed)
                          Vec3(y0, x0, z1), // bottomRight
                          Vec3(y0, x1, z1), // topRight
                          Vec3(y1, x1, z1), // topLeft
                          rgba
        );
    }

    // [FACE 3] Left face (reversed winding)
    {
        uint32_t colorVal = MultiplyColorBrightness(baseColor, BRIGHTNESS_Y_AXIS);
        Rgba8    rgba     = UnpackARGB32(colorVal);
        AddVertsForQuad3D(vertices,
                          Vec3(y0, x1, z0), // bottomLeft (reversed)
                          Vec3(y0, x0, z0), // bottomRight
                          Vec3(y0, x0, z1), // topRight
                          Vec3(y0, x1, z1), // topLeft
                          rgba
        );
    }

    // [FACE 4] Right face (reversed winding)
    {
        uint32_t colorVal = MultiplyColorBrightness(baseColor, BRIGHTNESS_Y_AXIS);
        Rgba8    rgba     = UnpackARGB32(colorVal);
        AddVertsForQuad3D(vertices,
                          Vec3(y1, x0, z0), // bottomLeft (reversed)
                          Vec3(y1, x1, z0), // bottomRight
                          Vec3(y1, x1, z1), // topRight
                          Vec3(y1, x0, z1), // topLeft
                          rgba
        );
    }

    // [FACE 5] Back face (reversed winding)
    {
        uint32_t colorVal = MultiplyColorBrightness(baseColor, BRIGHTNESS_X_AXIS);
        Rgba8    rgba     = UnpackARGB32(colorVal);
        AddVertsForQuad3D(vertices,
                          Vec3(y0, x0, z0), // bottomLeft (reversed)
                          Vec3(y1, x0, z0), // bottomRight
                          Vec3(y1, x0, z1), // topRight
                          Vec3(y0, x0, z1), // topLeft
                          rgba
        );
    }

    // [FACE 6] Front face (reversed winding)
    {
        uint32_t colorVal = MultiplyColorBrightness(baseColor, BRIGHTNESS_X_AXIS);
        Rgba8    rgba     = UnpackARGB32(colorVal);
        AddVertsForQuad3D(vertices,
                          Vec3(y1, x1, z0), // bottomLeft (reversed)
                          Vec3(y0, x1, z0), // bottomRight
                          Vec3(y0, x1, z1), // topRight
                          Vec3(y1, x1, z1), // topLeft
                          rgba
        );
    }
}
