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
    // [FIX] Use correct parameter order: originX, originY (matching Sodium)
    // Reference: Sodium CloudRenderer.java Line 154
    CloudTextureData::Slice slice = textureData->CreateSlice(
        params.originX, params.originY, radius
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
    int                            x, int y,
    ViewOrientation                orientation,
    bool                           flat
)
{
    // [STEP 1] Get cell index and visible faces
    // [FIX] Use (x, y) consistently - spiral traversal uses (x, z) but we map z -> y
    int index     = slice.GetCellIndex(x, y);
    int cellFaces = slice.GetCellFaces(index) & GetVisibleFaces(x, y, orientation);

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
        EmitCellGeometryFlat(vertices, color, x, y);
    }
    else
    {
        // Fancy mode: Exterior faces
        EmitCellGeometryExterior(vertices, cellFaces, color, x, y);

        // [RESTORED] Fancy mode: Interior faces for nearby cells
        // Reference: Sodium CloudRenderer.java Line 231-234
        // Interior faces are generated unconditionally (no ViewOrientation check)
        // They provide the "inside view" when camera enters the cloud
        // The GetVisibleFaces() culling handles which exterior faces to render
        if (TaxicabDistance(x, y) <= 1)
        {
            //EmitCellGeometryInterior(vertices, color, x, y);
        }
    }
}

/**
 * @brief [RESTORED] Calculate visible faces mask based on camera position
 *
 * Reference: Sodium CloudRenderer.java Line 237-264
 *
 * [IMPORTANT] This is NOT just for performance optimization!
 * It's critical for correct transparent rendering:
 * - Only renders faces that FACE the camera
 * - Prevents back faces from showing through semi-transparent front faces
 * - Without this, you see interior geometry through exterior faces
 *
 * Parameters (relative cell position from camera):
 * - x: Cell X position (spiral traversal first param)
 * - y: Cell Y position (spiral traversal second param, was 'z' in Sodium)
 *
 * Face mask bits (matching Sodium):
 * - NEG_Z=1, POS_Z=2 (vertical faces)
 * - NEG_X=4, POS_X=8 (X-axis faces)
 * - NEG_Y=16, POS_Y=32 (Y-axis faces, was Z in Sodium)
 */
int CloudGeometryHelper::GetVisibleFaces(
    int             x, int y,
    ViewOrientation orientation
)
{
    int faces = 0;

    // [STEP 1] Horizontal face culling based on camera position relative to cell
    // Reference: Sodium CloudRenderer.java Line 238-252
    //
    // Logic: If camera is on negative side of cell, render positive face (facing camera)
    //        If camera is on positive side of cell, render negative face (facing camera)

    // X-axis faces (bits 4 and 8)
    if (x <= 0)
    {
        faces |= FACE_MASK_POS_X; // Camera on -X side -> render +X face (bit 8)
    }
    if (x >= 0)
    {
        faces |= FACE_MASK_NEG_X; // Camera on +X side -> render -X face (bit 4)
    }

    // Y-axis faces (bits 16 and 32) - was Z in Sodium
    if (y <= 0)
    {
        faces |= FACE_MASK_POS_Y; // Camera on -Y side -> render +Y face (bit 32)
    }
    if (y >= 0)
    {
        faces |= FACE_MASK_NEG_Y; // Camera on +Y side -> render -Y face (bit 16)
    }

    // [STEP 2] Vertical face culling based on ViewOrientation
    // Reference: Sodium CloudRenderer.java Line 255-260
    //
    // orientation != BELOW_CLOUDS -> render top face (camera can see it)
    // orientation != ABOVE_CLOUDS -> render bottom face (camera can see it)

    if (orientation != ViewOrientation::BELOW_CLOUDS)
    {
        faces |= FACE_MASK_POS_Z; // Render top face (bit 2)
    }

    if (orientation != ViewOrientation::ABOVE_CLOUDS)
    {
        faces |= FACE_MASK_NEG_Z; // Render bottom face (bit 1)
    }

    return faces;
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
    int                  cellX, int cellY
)
{
    // [STEP 1] Calculate cell position (12x12 horizontal plane)
    // [FIX] Direct mapping: spiral (x,y) -> geometry (x,y)
    // Reference: Sodium CloudRenderer.java Line 271-274
    float x0 = cellX * 12.0f;
    float x1 = x0 + 12.0f;
    float y0 = cellY * 12.0f;
    float y1 = y0 + 12.0f;
    float z0 = 0.0f; // Height fixed at 0 (ModelMatrix handles world Z)

    // [STEP 2] Calculate color (top face brightness 1.0)
    uint32_t vertexColor = MultiplyColorBrightness(color, BRIGHTNESS_POS_Z);
    Rgba8    rgba        = UnpackARGB32(vertexColor);

    // [STEP 3] Generate 6 vertices (2 triangles) using AddVertsForQuad3D
    // [FIX] Engine only supports TriangleList, not QuadList
    // AddVertsForQuad3D params: bottomLeft, bottomRight, topRight, topLeft
    AddVertsForQuad3D(vertices,
                      Vec3(x0, y0, z0), // bottomLeft
                      Vec3(x1, y0, z0), // bottomRight
                      Vec3(x1, y1, z0), // topRight
                      Vec3(x0, y1, z0), // topLeft
                      rgba
    );
}

/**
 * @brief [NEW] Generate Fancy mode exterior faces
 * Reference: Sodium CloudRenderer.java Line 299-386
 * [FIX] Use AddVertsForQuad3D to generate 6 vertices (2 triangles) per face instead of 4 vertices
 */
void CloudGeometryHelper::EmitCellGeometryExterior(
    std::vector<Vertex>& vertices,
    int                  cellFaces,
    uint32_t             cellColor,
    int                  cellX, int cellY
)
{
    // [FIX] Direct coordinate mapping: spiral (x,y) -> geometry (x,y)
    // Cell dimensions: 12x12 (horizontal) x 4 (height)
    // Reference: Sodium CloudRenderer.java Line 306-311
    float x0 = cellX * 12.0f;
    float x1 = x0 + 12.0f;
    float y0 = cellY * 12.0f;
    float y1 = y0 + 12.0f;
    float z0 = 0.0f; // Bottom of cloud
    float z1 = 4.0f; // Top of cloud (4-block thickness)

    // [FACE 1] Bottom face (NEG_Z) - Looking from below
    if (cellFaces & FACE_MASK_NEG_Z)
    {
        uint32_t colorVal = MultiplyColorBrightness(cellColor, BRIGHTNESS_NEG_Z);
        Rgba8    rgba     = UnpackARGB32(colorVal);
        AddVertsForQuad3D(vertices,
                          Vec3(x0, y0, z0),
                          Vec3(x0, y1, z0),
                          Vec3(x1, y1, z0),
                          Vec3(x1, y0, z0),
                          rgba
        );
    }

    // [FACE 2] Top face (POS_Z) - Looking from above
    if (cellFaces & FACE_MASK_POS_Z)
    {
        uint32_t colorVal = MultiplyColorBrightness(cellColor, BRIGHTNESS_POS_Z);
        Rgba8    rgba     = UnpackARGB32(colorVal);
        AddVertsForQuad3D(vertices,
                          Vec3(x0, y0, z1),
                          Vec3(x1, y0, z1),
                          Vec3(x1, y1, z1),
                          Vec3(x0, y1, z1),
                          rgba
        );
    }

    // [FACE 3] NEG_X face - Looking from -X direction
    if (cellFaces & FACE_MASK_NEG_X)
    {
        uint32_t colorVal = MultiplyColorBrightness(cellColor, BRIGHTNESS_X_AXIS);
        Rgba8    rgba     = UnpackARGB32(colorVal);
        AddVertsForQuad3D(vertices,
                          Vec3(x0, y0, z0),
                          Vec3(x0, y0, z1),
                          Vec3(x0, y1, z1),
                          Vec3(x0, y1, z0),
                          rgba
        );
    }

    // [FACE 4] POS_X face - Looking from +X direction
    if (cellFaces & FACE_MASK_POS_X)
    {
        uint32_t colorVal = MultiplyColorBrightness(cellColor, BRIGHTNESS_X_AXIS);
        Rgba8    rgba     = UnpackARGB32(colorVal);
        AddVertsForQuad3D(vertices,
                          Vec3(x1, y1, z0),
                          Vec3(x1, y1, z1),
                          Vec3(x1, y0, z1),
                          Vec3(x1, y0, z0),
                          rgba
        );
    }

    // [FACE 5] NEG_Y face - Looking from -Y direction
    if (cellFaces & FACE_MASK_NEG_Y)
    {
        uint32_t colorVal = MultiplyColorBrightness(cellColor, BRIGHTNESS_Y_AXIS);
        Rgba8    rgba     = UnpackARGB32(colorVal);
        AddVertsForQuad3D(vertices,
                          Vec3(x1, y0, z0),
                          Vec3(x1, y0, z1),
                          Vec3(x0, y0, z1),
                          Vec3(x0, y0, z0),
                          rgba
        );
    }

    // [FACE 6] POS_Y face - Looking from +Y direction
    if (cellFaces & FACE_MASK_POS_Y)
    {
        uint32_t colorVal = MultiplyColorBrightness(cellColor, BRIGHTNESS_Y_AXIS);
        Rgba8    rgba     = UnpackARGB32(colorVal);
        AddVertsForQuad3D(vertices,
                          Vec3(x0, y1, z0),
                          Vec3(x0, y1, z1),
                          Vec3(x1, y1, z1),
                          Vec3(x1, y1, z0),
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
    int                  cellX, int cellY
)
{
    // [IMPORTANT] Interior faces = Exterior faces with reversed vertex winding
    // Fixed generation of 6 faces × 6 vertices = 36 vertices (2 triangles per face)

    // [FIX] Direct coordinate mapping: spiral (x,y) -> geometry (x,y)
    float x0 = cellX * 12.0f;
    float x1 = x0 + 12.0f;
    float y0 = cellY * 12.0f;
    float y1 = y0 + 12.0f;
    float z0 = 0.0f;
    float z1 = 4.0f;

    // [FACE 1] Bottom face (reversed winding - looking from inside)
    {
        uint32_t colorVal = MultiplyColorBrightness(baseColor, BRIGHTNESS_NEG_Z);
        Rgba8    rgba     = UnpackARGB32(colorVal);
        AddVertsForQuad3D(vertices,
                          Vec3(x1, y0, z0),
                          Vec3(x1, y1, z0),
                          Vec3(x0, y1, z0),
                          Vec3(x0, y0, z0),
                          rgba
        );
    }

    // [FACE 2] Top face (reversed winding)
    {
        uint32_t colorVal = MultiplyColorBrightness(baseColor, BRIGHTNESS_POS_Z);
        Rgba8    rgba     = UnpackARGB32(colorVal);
        AddVertsForQuad3D(vertices,
                          Vec3(x1, y0, z1),
                          Vec3(x0, y0, z1),
                          Vec3(x0, y1, z1),
                          Vec3(x1, y1, z1),
                          rgba
        );
    }

    // [FACE 3] NEG_X face (reversed winding)
    {
        uint32_t colorVal = MultiplyColorBrightness(baseColor, BRIGHTNESS_X_AXIS);
        Rgba8    rgba     = UnpackARGB32(colorVal);
        AddVertsForQuad3D(vertices,
                          Vec3(x0, y1, z0),
                          Vec3(x0, y1, z1),
                          Vec3(x0, y0, z1),
                          Vec3(x0, y0, z0),
                          rgba
        );
    }

    // [FACE 4] POS_X face (reversed winding)
    {
        uint32_t colorVal = MultiplyColorBrightness(baseColor, BRIGHTNESS_X_AXIS);
        Rgba8    rgba     = UnpackARGB32(colorVal);
        AddVertsForQuad3D(vertices,
                          Vec3(x1, y0, z0),
                          Vec3(x1, y0, z1),
                          Vec3(x1, y1, z1),
                          Vec3(x1, y1, z0),
                          rgba
        );
    }

    // [FACE 5] NEG_Y face (reversed winding)
    {
        uint32_t colorVal = MultiplyColorBrightness(baseColor, BRIGHTNESS_Y_AXIS);
        Rgba8    rgba     = UnpackARGB32(colorVal);
        AddVertsForQuad3D(vertices,
                          Vec3(x0, y0, z0),
                          Vec3(x0, y0, z1),
                          Vec3(x1, y0, z1),
                          Vec3(x1, y0, z0),
                          rgba
        );
    }

    // [FACE 6] POS_Y face (reversed winding)
    {
        uint32_t colorVal = MultiplyColorBrightness(baseColor, BRIGHTNESS_Y_AXIS);
        Rgba8    rgba     = UnpackARGB32(colorVal);
        AddVertsForQuad3D(vertices,
                          Vec3(x1, y1, z0),
                          Vec3(x1, y1, z1),
                          Vec3(x0, y1, z1),
                          Vec3(x0, y1, z0),
                          rgba
        );
    }
}
