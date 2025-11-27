#include "Game/Framework/RenderPass/RenderSky/SkyGeometryHelper.hpp"
#include "Engine/Math/MathUtils.hpp"
#include <cmath>

//-----------------------------------------------------------------------------------------------
std::vector<Vertex> SkyGeometryHelper::GenerateSkyDisc(float centerZ, const Rgba8& color)
{
    // [REQUIRED] 10 vertices total: 1 center + 9 perimeter
    std::vector<Vertex> vertices;
    vertices.reserve(10);

    // [IMPORTANT] Minecraft vanilla specs (LevelRenderer.java:571-582)
    constexpr float RADIUS = 512.0f;           // Perimeter radius
    constexpr float ANGLE_STEP = 45.0f;        // 45° step between perimeter vertices
    constexpr float DEG_TO_RAD = 0.017453292f; // π/180 (Minecraft uses this constant)

    // [NEW] Step 1: Add center vertex (TRIANGLE_FAN starts with center)
    // Coordinate conversion: Minecraft (0, centerZ, 0) -> Engine (0, 0, centerZ)
    Vec3 centerPosition(0.0f, 0.0f, centerZ);
    Vec2 centerUV(0.5f, 0.5f); // Center of texture
    Vec3 normalUp(0.0f, 0.0f, 1.0f); // Normal points up (+Z)

    vertices.emplace_back(
        centerPosition,
        color,
        centerUV,
        normalUp,
        Vec3(1.0f, 0.0f, 0.0f), // Tangent (arbitrary for sky disc)
        Vec3(0.0f, 1.0f, 0.0f)  // Bitangent (arbitrary for sky disc)
    );

    // [NEW] Step 2: Add 9 perimeter vertices from -180° to +180° (45° steps)
    // Minecraft loop: for(int i = -180; i <= 180; i += 45)
    for (int angleDeg = -180; angleDeg <= 180; angleDeg += static_cast<int>(ANGLE_STEP))
    {
        float angleRad = static_cast<float>(angleDeg) * DEG_TO_RAD;

        // [IMPORTANT] Minecraft formula (LevelRenderer.java:578):
        // X = sign(centerZ) * 512.0 * cos(angle)
        // Y = centerZ
        // Z = 512.0 * sin(angle)
        float signCenterZ = (centerZ >= 0.0f) ? 1.0f : -1.0f;
        float mcX = signCenterZ * RADIUS * std::cos(angleRad);
        float mcY = centerZ;
        float mcZ = RADIUS * std::sin(angleRad);

        // [IMPORTANT] Coordinate system conversion: (MC_X, MC_Y, MC_Z) -> (Engine_X, Engine_Z, Engine_Y)
        Vec3 perimeterPosition(mcX, mcZ, mcY);

        // [GOOD] UV mapping: map angle to [0, 1] range
        float u = (static_cast<float>(angleDeg) + 180.0f) / 360.0f; // [-180, 180] -> [0, 1]
        float v = 1.0f; // Perimeter vertices at edge of texture
        Vec2 perimeterUV(u, v);

        vertices.emplace_back(
            perimeterPosition,
            color,
            perimeterUV,
            normalUp,
            Vec3(1.0f, 0.0f, 0.0f), // Tangent (arbitrary for sky disc)
            Vec3(0.0f, 1.0f, 0.0f)  // Bitangent (arbitrary for sky disc)
        );
    }

    // [DONE] Verify vertex count (should be exactly 10)
    // 1 center + 9 perimeter = 10 vertices
    return vertices;
}
