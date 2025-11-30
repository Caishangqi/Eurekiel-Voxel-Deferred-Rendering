#include "Game/Framework/RenderPass/RenderSky/SkyGeometryHelper.hpp"
#include "Engine/Math/MathUtils.hpp"
#include <cmath>

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

    // [IMPORTANT] Minecraft vanilla specs
    constexpr float RADIUS     = 32.0f; // Perimeter radius (horizontal distance)
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
    // [Sunrise/Sunset Strip] - Minecraft Vanilla Style (Fixed)
    // ==========================================================================
    // Geometry: TRIANGLE_FAN converted to TRIANGLE_LIST
    // - Center point at (0, 0, 0) facing +X direction (forward)
    // - 16 outer vertices forming a semi-circle arc
    // - Alpha gradient: center (high alpha) -> outer (zero alpha)
    // - NO rotation here - CPU matrix transform applied by caller
    // - Matches sky dome radius and structure
    // ==========================================================================

    std::vector<Vertex> vertices;
    vertices.reserve(48); // 16 triangles * 3 vertices

    constexpr int   SEGMENTS = 16;
    constexpr float RADIUS   = 32.0f; // Match sky dome radius
    constexpr float TWO_PI   = 6.28318530718f;
    constexpr float HALF_PI  = 1.57079632679f; // 90 degrees

    // ==========================================================================
    // [STEP 1] Define center vertex at origin (no height offset)
    // ==========================================================================
    // Strip center at (0, 0, 0) - this will be rotated to horizon position
    // by CPU matrix transform (not geometry-level modification)
    Vec3 centerPosition(0.0f, 0.0f, 0.0f);

    // Center color: full sunriseColor with original alpha (typically 0.8)
    Rgba8 centerColor = Rgba8(
        static_cast<unsigned char>(sunriseColor.x * 255.0f),
        static_cast<unsigned char>(sunriseColor.y * 255.0f),
        static_cast<unsigned char>(sunriseColor.z * 255.0f),
        static_cast<unsigned char>(sunriseColor.w * 255.0f)
    );

    // Outer color: same RGB but alpha = 0 (transparent)
    Rgba8 outerColor = Rgba8(
        static_cast<unsigned char>(sunriseColor.x * 255.0f),
        static_cast<unsigned char>(sunriseColor.y * 255.0f),
        static_cast<unsigned char>(sunriseColor.z * 255.0f),
        0
    );

    // Default vertex attributes
    Vec2 defaultUV(0.5f, 0.5f);
    Vec3 normal(0.0f, 0.0f, 1.0f);
    Vec3 tangent(1.0f, 0.0f, 0.0f);
    Vec3 bitangent(0.0f, 1.0f, 0.0f);

    // ==========================================================================
    // [STEP 2] Generate outer vertices in semi-circle arc
    // ==========================================================================
    // Arc from -90° to +90° (left to right in Y-Z plane)
    // This creates a fan shape facing +X direction (engine forward)
    std::vector<Vec3> outerPositions;
    outerPositions.reserve(SEGMENTS + 1);

    for (int i = 0; i <= SEGMENTS; ++i)
    {
        // Map i to angle range [-90°, +90°]
        float t     = static_cast<float>(i) / static_cast<float>(SEGMENTS); // 0.0 to 1.0
        float angle = -HALF_PI + t * (TWO_PI / 2.0f); // -π/2 to +π/2

        // Outer vertices in Y-Z plane (X=0), forming arc
        float y = std::cos(angle) * RADIUS; // -RADIUS to +RADIUS
        float z = std::sin(angle) * RADIUS; // -RADIUS to +RADIUS
        float x = 0.0f; // On Y-Z plane

        outerPositions.emplace_back(x, y, z);
    }

    // ==========================================================================
    // [STEP 3] Generate TRIANGLE_LIST (16 triangles)
    // ==========================================================================
    // Each triangle: center -> outer[i] -> outer[i+1]
    for (int i = 0; i < SEGMENTS; ++i)
    {
        // Triangle vertices (CCW winding for front-facing)
        // Vertex 1: Center
        vertices.emplace_back(centerPosition, centerColor, defaultUV, normal, tangent, bitangent);

        // Vertex 2: Outer[i]
        vertices.emplace_back(outerPositions[i], outerColor, defaultUV, normal, tangent, bitangent);

        // Vertex 3: Outer[i+1]
        vertices.emplace_back(outerPositions[i + 1], outerColor, defaultUV, normal, tangent, bitangent);
    }

    // [DONE] 16 triangles * 3 vertices = 48 vertices
    // Rotation to sun/sunset position will be applied by caller using Mat44 transform
    return vertices;
}
