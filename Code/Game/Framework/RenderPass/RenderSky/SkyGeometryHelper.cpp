#include "Game/Framework/RenderPass/RenderSky/SkyGeometryHelper.hpp"
#include "Engine/Math/MathUtils.hpp"
#include <cmath>

#include "Game/GameCommon.hpp"

//-----------------------------------------------------------------------------------------------
std::vector<Vertex> SkyGeometryHelper::GenerateSkyDisc(float centerZ, const Rgba8& color)
{
    // ==========================================================================
    // [FIX] Sky Dome Geometry - Inverted Bowl Shape
    // ==========================================================================
    // Problem: Previous implementation created a FLAT disc at Z=centerZ
    //          Player at (0,0,0) was BELOW the disc and couldn't see it
    //
    // Solution: Create an INVERTED BOWL (dome) shape:
    //          - Center point at TOP (Z = +centerZ, above player)
    //          - Perimeter points at HORIZON level (Z = 0, around player)
    //          - Player looks UP from INSIDE the dome to see sky
    //
    // Minecraft Reference: LevelRenderer.java:571-582
    // ==========================================================================

    std::vector<Vertex> vertices;
    vertices.reserve(24); // 8 triangles * 3 vertices

    // ==========================================================================
    // [Minecraft Vanilla Specs] LevelRenderer.java:571-582
    // ==========================================================================
    // buildSkyDisc(tesselator, 16.0f):
    //   float g = Math.signum(f) * 512.0F;  // Radius = 512
    //   float h = 512.0F;
    //   bufferBuilder.addVertex(0.0F, f, 0.0F);  // Center at (0, ±16, 0)
    //   for(int i = -180; i <= 180; i += 45) {   // 45° step
    //       bufferBuilder.addVertex(g * cos(i), f, 512 * sin(i));
    //   }
    // ==========================================================================
    constexpr float RADIUS     = 512.0f; // [FIX] Minecraft uses 512.0, not 32.0!
    constexpr float ANGLE_STEP = 45.0f; // 45 degree step between perimeter vertices
    constexpr float DEG_TO_RAD = 0.017453292f; // PI/180

    // ==========================================================================
    // [FIX] Center vertex at TOP of dome (zenith)
    // ==========================================================================
    // centerZ > 0: Sky dome (center above player)
    // centerZ < 0: Void dome (center below player)
    Vec3 centerPosition(0.0f, 0.0f, centerZ);
    Vec2 centerUV(0.5f, 0.5f);

    // Normal points INWARD (toward player) for correct lighting
    Vec3 normalDown(0.0f, 0.0f, -1.0f);
    Vec3 tangent(1.0f, 0.0f, 0.0f);
    Vec3 bitangent(0.0f, 1.0f, 0.0f);

    // [Step 1] Pre-calculate all 9 perimeter vertices at HORIZON level
    std::vector<Vec3> perimeterPositions;
    std::vector<Vec2> perimeterUVs;
    perimeterPositions.reserve(9);
    perimeterUVs.reserve(9);

    for (int angleDeg = -180; angleDeg <= 180; angleDeg += static_cast<int>(ANGLE_STEP))
    {
        float angleRad = static_cast<float>(angleDeg) * DEG_TO_RAD;

        // ==========================================================================
        // [FIX] Perimeter vertices at HORIZON (Z = 0)
        // ==========================================================================
        // X = RADIUS * cos(angle) - horizontal position
        // Y = RADIUS * sin(angle) - horizontal position
        // Z = 0 - at horizon level (same height as player)
        //
        // This creates a dome shape:
        //     Center (0, 0, +16) at zenith
        //           /    |    \
        //          /     |     \
        //    (-512,0,0)--+---(+512,0,0) at horizon
        // ==========================================================================
        float x = RADIUS * std::cos(angleRad);
        float y = RADIUS * std::sin(angleRad);
        float z = 0.0f; // Horizon level

        perimeterPositions.emplace_back(x, y, z);

        // UV mapping: angle to U, center-to-edge to V
        float u = (static_cast<float>(angleDeg) + 180.0f) / 360.0f;
        float v = 1.0f;
        perimeterUVs.emplace_back(u, v);
    }

    // ==========================================================================
    // [Step 2] Generate TRIANGLE_LIST with CORRECT winding order
    // ==========================================================================
    // Winding order depends on which side player views from:
    // - Upper hemisphere (centerZ > 0): Player looks UP from below
    //   -> CCW from below = Center -> Next -> Current
    // - Lower hemisphere (centerZ < 0): Player looks DOWN from above
    //   -> CCW from above = Center -> Current -> Next (opposite order)
    // ==========================================================================
    bool isUpperHemisphere = (centerZ > 0.0f);

    // Normal direction: always point toward player (inward)
    Vec3 normal = isUpperHemisphere ? Vec3(0.0f, 0.0f, -1.0f) : Vec3(0.0f, 0.0f, 1.0f);

    for (size_t i = 0; i < perimeterPositions.size() - 1; ++i)
    {
        if (isUpperHemisphere)
        {
            // Upper hemisphere: CCW from below (player looks up)
            // Order: Center -> Next -> Current
            vertices.emplace_back(centerPosition, color, centerUV, normal, tangent, bitangent);
            vertices.emplace_back(perimeterPositions[i + 1], color, perimeterUVs[i + 1], normal, tangent, bitangent);
            vertices.emplace_back(perimeterPositions[i], color, perimeterUVs[i], normal, tangent, bitangent);
        }
        else
        {
            // Lower hemisphere: CCW from above (player looks down)
            // Order: Center -> Current -> Next (reversed)
            vertices.emplace_back(centerPosition, color, centerUV, normal, tangent, bitangent);
            vertices.emplace_back(perimeterPositions[i], color, perimeterUVs[i], normal, tangent, bitangent);
            vertices.emplace_back(perimeterPositions[i + 1], color, perimeterUVs[i + 1], normal, tangent, bitangent);
        }
    }

    // [DONE] Vertex count: 8 triangles * 3 = 24 vertices
    return vertices;
}

//-----------------------------------------------------------------------------------------------
std::vector<Vertex> SkyGeometryHelper::GenerateCelestialBillboard(
    float        celestialType,
    const AABB2& uvBounds,
    const Rgba8& color)
{
    // [REQUIRED] 6 vertices for 2 triangles (quad)
    std::vector<Vertex> vertices;
    vertices.reserve(6);

    // [STEP 1] Define billboard corner offsets (-0.5 to 0.5)
    // These offsets will be used by the vertex shader to calculate billboard world position
    Vec3 topLeft(-0.5f, 0.5f, celestialType);
    Vec3 topRight(0.5f, 0.5f, celestialType);
    Vec3 bottomLeft(-0.5f, -0.5f, celestialType);
    Vec3 bottomRight(0.5f, -0.5f, celestialType);

    // [STEP 2] Extract UV coordinates from atlas bounds (CPU-side calculation)
    Vec2 uvMin = uvBounds.m_mins;
    Vec2 uvMax = uvBounds.m_maxs;

    // [STEP 3] Define UV coordinates for each corner
    Vec2 uvTopLeft(uvMin.x, uvMin.y);
    Vec2 uvTopRight(uvMax.x, uvMin.y);
    Vec2 uvBottomLeft(uvMin.x, uvMax.y);
    Vec2 uvBottomRight(uvMax.x, uvMax.y);

    // [STEP 4] Define normal, tangent, bitangent (arbitrary for billboard)
    Vec3 normal(0.0f, 0.0f, 1.0f);
    Vec3 tangent(1.0f, 0.0f, 0.0f);
    Vec3 bitangent(0.0f, 1.0f, 0.0f);

    // [STEP 5] Generate 6 vertices for 2 triangles (CCW winding)
    // Triangle 1: Top-Left, Bottom-Left, Bottom-Right
    vertices.emplace_back(topLeft, color, uvTopLeft, normal, tangent, bitangent);
    vertices.emplace_back(bottomLeft, color, uvBottomLeft, normal, tangent, bitangent);
    vertices.emplace_back(bottomRight, color, uvBottomRight, normal, tangent, bitangent);

    // Triangle 2: Top-Left, Bottom-Right, Top-Right
    vertices.emplace_back(topLeft, color, uvTopLeft, normal, tangent, bitangent);
    vertices.emplace_back(bottomRight, color, uvBottomRight, normal, tangent, bitangent);
    vertices.emplace_back(topRight, color, uvTopRight, normal, tangent, bitangent);

    return vertices;
}

//-----------------------------------------------------------------------------------------------
std::vector<Vertex> SkyGeometryHelper::GenerateSunriseStrip(const Vec4& sunriseColor)
{
    // ==========================================================================
    // [Sunrise/Sunset Strip] - Engine Coordinate System Version
    // ==========================================================================
    //
    // [Original Minecraft Geometry] (Y-up coordinate system):
    //   Center: (0, 100, 0) - 100 units UP (+Y)
    //   Outer:  X = sin(theta) * 120  (horizontal left-right)
    //           Y = cos(theta) * 120  (vertical up-down)
    //           Z = -cos(theta) * 40 * alpha (depth toward/away)
    //
    // [Minecraft Transform] XP(90) -> ZP(flip) -> ZP(90):
    //   This rotates the vertical fan to horizontal, facing sunrise/sunset
    //
    // [Final Position in Camera Space]:
    //   The strip becomes a HORIZONTAL ARC at the HORIZON
    //   - Center point: in the +X direction (forward toward horizon)
    //   - Arc spans: left-right along Y axis
    //   - Depth variation: creates the "bowl" effect
    //
    // ==========================================================================
    // [ENGINE COORDINATE SYSTEM] (Z-up, X-forward, Y-left):
    //
    //   After all transforms, the strip should be positioned:
    //   - Center: somewhere on the horizon ring, toward sunrise/sunset
    //   - Arc: horizontal band spanning left-right
    //   - Player looks toward +X to see the strip
    //
    //   We generate directly in ENGINE SPACE, no coordinate conversion needed!
    //   The SkyRenderPass only needs to apply camera view rotation.
    // ==========================================================================

    std::vector<Vertex> vertices;
    vertices.reserve(51); // 17 triangles * 3 vertices

    // ==========================================================================
    // [Minecraft Vanilla Specs] LevelRenderer.java:1538-1546
    // ==========================================================================
    // bufferBuilder.addVertex(matrix4f3, 0.0F, 100.0F, 0.0F)           // Center
    // for(int o = 0; o <= 16; ++o) {
    //     p = (float)o * 6.2831855F / 16.0F;                           // 16 segments
    //     q = Mth.sin(p);
    //     r = Mth.cos(p);
    //     bufferBuilder.addVertex(q * 120.0F, r * 120.0F, -r * 40.0F * fs[3]);
    // }
    // ==========================================================================
    constexpr int   SEGMENTS    = 16;
    constexpr float CENTER_DIST = 100.0f; // [FIX] Minecraft: (0, 100, 0)
    constexpr float OUTER_DIST  = 120.0f; // [FIX] Minecraft: sin*120, cos*120
    constexpr float DEPTH_SCALE = 40.0f; // [FIX] Minecraft: -cos*40*alpha
    constexpr float TWO_PI      = 6.28318530718f;

    // ==========================================================================
    // [ENGINE SPACE] Strip Geometry
    // ==========================================================================
    // After Minecraft's XP(90)->ZP(90) transform, the geometry becomes:
    //   - The "up" direction (MC +Y) becomes "forward" (Engine +X)
    //   - The "right" direction (MC +X) becomes "left" (Engine +Y)
    //   - The "toward" direction (MC -Z) becomes "up" (Engine +Z)
    //
    // So in ENGINE SPACE:
    //   Center: (CENTER_DIST, 0, 0) - toward +X (forward/horizon)
    //   Outer arc: spans in Y direction (left-right)
    //              with X variation (depth toward horizon)
    //              and Z variation (height, the "bowl" curve)
    // ==========================================================================

    // Center position: forward toward horizon (+X direction)
    Vec3 centerPosition(CENTER_DIST, 0.0f, 0.0f);

    // Center color: full sunriseColor with original alpha
    Rgba8 centerColor = Rgba8(
        static_cast<unsigned char>(sunriseColor.x * 255.0f),
        static_cast<unsigned char>(sunriseColor.y * 255.0f),
        static_cast<unsigned char>(sunriseColor.z * 255.0f),
        static_cast<unsigned char>(sunriseColor.w * 255.0f)
    );

    // Outer color: same RGB but alpha = 0 (transparent edge)
    Rgba8 outerColor = Rgba8(
        static_cast<unsigned char>(sunriseColor.x * 255.0f),
        static_cast<unsigned char>(sunriseColor.y * 255.0f),
        static_cast<unsigned char>(sunriseColor.z * 255.0f),
        0
    );

    // Default vertex attributes
    Vec2 defaultUV(0.5f, 0.5f);
    Vec3 normal(-1.0f, 0.0f, 0.0f); // Facing back toward player (-X)
    Vec3 tangent(0.0f, 1.0f, 0.0f);
    Vec3 bitangent(0.0f, 0.0f, 1.0f);

    // ==========================================================================
    // [STEP 2] Generate outer vertices in ENGINE SPACE
    // ==========================================================================
    // Mapping from Minecraft post-transform to Engine:
    //   MC X (after transform) -> Engine Y (left-right)
    //   MC Y (after transform) -> Engine X (forward, toward horizon)
    //   MC Z (after transform) -> Engine Z (up-down)
    //
    // Original MC outer: (sin*120, cos*120, -cos*40*alpha)
    // After XP(90): Y->-Z, Z->Y => (sin*120, -(-cos*40*alpha), -cos*120)
    //             = (sin*120, cos*40*alpha, -cos*120)
    // After ZP(90): X->-Y, Y->X => (-cos*40*alpha, sin*120, -cos*120)
    //
    // In Engine space:
    //   X (forward) = cos(theta) * DEPTH_SCALE * alpha  (depth toward horizon)
    //   Y (left)    = sin(theta) * OUTER_DIST           (horizontal span)
    //   Z (up)      = -cos(theta) * OUTER_DIST          (vertical curve)
    // ==========================================================================
    std::vector<Vec3> outerPositions;
    outerPositions.reserve(SEGMENTS + 1);

    float depthOffset = DEPTH_SCALE * sunriseColor.w; // Scale by alpha

    for (int i = 0; i <= SEGMENTS; ++i)
    {
        float angle = static_cast<float>(i) * TWO_PI / static_cast<float>(SEGMENTS);
        float sinA  = std::sin(angle);
        float cosA  = std::cos(angle);

        // Engine space coordinates (derived from MC transform)
        float x = cosA * depthOffset; // Forward depth (toward horizon)
        float y = sinA * OUTER_DIST; // Left-right span
        float z = -cosA * OUTER_DIST; // Up-down curve (inverted bowl)

        outerPositions.emplace_back(x, y, z);
    }

    // ==========================================================================
    // [STEP 3] Generate TRIANGLE_LIST (16 triangles)
    // ==========================================================================
    for (int i = 0; i < SEGMENTS; ++i)
    {
        // CCW winding when viewed from behind (player at origin looking toward +X)
        vertices.emplace_back(centerPosition, centerColor, defaultUV, normal, tangent, bitangent);
        vertices.emplace_back(outerPositions[i], outerColor, defaultUV, normal, tangent, bitangent);
        vertices.emplace_back(outerPositions[i + 1], outerColor, defaultUV, normal, tangent, bitangent);
    }

    // [DONE] 16 triangles * 3 vertices = 48 vertices
    return vertices;
}
