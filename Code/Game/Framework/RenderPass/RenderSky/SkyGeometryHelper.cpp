#include "Game/Framework/RenderPass/RenderSky/SkyGeometryHelper.hpp"
#include "Game/Framework/RenderPass/RenderSky/SkyColorHelper.hpp" // [NEW] For fog blending
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/Mat44.hpp"
#include <cmath>

#include "Engine/Core/VertexUtils.hpp"
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
    std::vector<Vec3>  perimeterPositions;
    std::vector<Vec2>  perimeterUVs;
    std::vector<Rgba8> perimeterColors; // [NEW] Store per-vertex colors for alpha gradient
    perimeterPositions.reserve(9);
    perimeterUVs.reserve(9);
    perimeterColors.reserve(9);

    // [FIX] Moved declaration before first use in loop below
    bool isUpperHemisphere = (centerZ > 0.0f);

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

        // [NEW] Alpha gradient for sky dome edge transparency
        // Reference: Minecraft sky dome has semi-transparent edges at horizon
        // Upper hemisphere: edge alpha = 0 (fully transparent at horizon)
        // Lower hemisphere: keep opaque (void dome doesn't need transparency)
        unsigned char edgeAlpha = isUpperHemisphere ? 0 : 255;
        perimeterColors.emplace_back(color.r, color.g, color.b, edgeAlpha);
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

    // Normal direction: always point toward player (inward)
    Vec3 normal = isUpperHemisphere ? Vec3(0.0f, 0.0f, -1.0f) : Vec3(0.0f, 0.0f, 1.0f);

    for (size_t i = 0; i < perimeterPositions.size() - 1; ++i)
    {
        if (isUpperHemisphere)
        {
            // Upper hemisphere: CCW from below (player looks up)
            // Order: Center -> Next -> Current
            // [NEW] Center vertex uses full alpha, perimeter vertices use gradient alpha
            vertices.emplace_back(centerPosition, color, centerUV, normal, tangent, bitangent);
            vertices.emplace_back(perimeterPositions[i + 1], perimeterColors[i + 1], perimeterUVs[i + 1], normal, tangent, bitangent);
            vertices.emplace_back(perimeterPositions[i], perimeterColors[i], perimeterUVs[i], normal, tangent, bitangent);
        }
        else
        {
            // Lower hemisphere: CCW from above (player looks down)
            // Order: Center -> Current -> Next (reversed)
            // [NEW] Void dome keeps full opacity (no alpha gradient needed)
            vertices.emplace_back(centerPosition, color, centerUV, normal, tangent, bitangent);
            vertices.emplace_back(perimeterPositions[i], perimeterColors[i], perimeterUVs[i], normal, tangent, bitangent);
            vertices.emplace_back(perimeterPositions[i + 1], perimeterColors[i + 1], perimeterUVs[i + 1], normal, tangent, bitangent);
        }
    }

    // [DONE] Vertex count: 8 triangles * 3 = 24 vertices
    return vertices;
}

//-----------------------------------------------------------------------------------------------
// [NEW] Generate sky disc with CPU-side fog blending (Iris-style)
// Each vertex color is calculated based on its elevation angle:
// - Center (zenith, elevation=90°): pure skyColor
// - Perimeter (horizon, elevation=0°): pure fogColor
// GPU interpolation creates smooth gradient between vertices
std::vector<Vertex> SkyGeometryHelper::GenerateSkyDiscWithFog(float centerZ, float celestialAngle)
{
    std::vector<Vertex> vertices;
    vertices.reserve(24); // 8 triangles * 3 vertices

    constexpr float RADIUS     = 512.0f;
    constexpr float ANGLE_STEP = 45.0f;
    constexpr float DEG_TO_RAD = 0.017453292f;

    // ==========================================================================
    // [Step 1] Calculate center vertex color (zenith = pure skyColor)
    // ==========================================================================
    Vec3 centerPosition(0.0f, 0.0f, centerZ);

    // Center vertex elevation = 90° (looking straight up)
    float centerElevation = SkyColorHelper::CalculateElevationAngle(centerPosition);
    Vec3  centerColorVec  = SkyColorHelper::CalculateSkyColorWithFog(celestialAngle, centerElevation);

    Rgba8 centerColor(
        static_cast<unsigned char>(centerColorVec.x * 255.0f),
        static_cast<unsigned char>(centerColorVec.y * 255.0f),
        static_cast<unsigned char>(centerColorVec.z * 255.0f),
        255 // Fully opaque at center
    );

    Vec2 centerUV(0.5f, 0.5f);

    // Normal points INWARD (toward player) for correct lighting
    bool isUpperHemisphere = (centerZ > 0.0f);
    Vec3 normal            = isUpperHemisphere ? Vec3(0.0f, 0.0f, -1.0f) : Vec3(0.0f, 0.0f, 1.0f);
    Vec3 tangent(1.0f, 0.0f, 0.0f);
    Vec3 bitangent(0.0f, 1.0f, 0.0f);

    // ==========================================================================
    // [Step 2] Calculate perimeter vertices (horizon = pure fogColor)
    // ==========================================================================
    std::vector<Vec3>  perimeterPositions;
    std::vector<Vec2>  perimeterUVs;
    std::vector<Rgba8> perimeterColors;
    perimeterPositions.reserve(9);
    perimeterUVs.reserve(9);
    perimeterColors.reserve(9);

    for (int angleDeg = -180; angleDeg <= 180; angleDeg += static_cast<int>(ANGLE_STEP))
    {
        float angleRad = static_cast<float>(angleDeg) * DEG_TO_RAD;

        // Perimeter position at horizon (Z = 0)
        float x = RADIUS * std::cos(angleRad);
        float y = RADIUS * std::sin(angleRad);
        float z = 0.0f;

        Vec3 perimeterPos(x, y, z);
        perimeterPositions.push_back(perimeterPos);

        // UV mapping
        float u = (static_cast<float>(angleDeg) + 180.0f) / 360.0f;
        float v = 1.0f;
        perimeterUVs.emplace_back(u, v);

        // Calculate fog-blended color based on elevation
        // [FIX] Perimeter is at horizon (Z=0), so elevation = 0° -> pure fogColor
        float perimeterElevation = SkyColorHelper::CalculateElevationAngle(perimeterPos);
        Vec3  perimeterColorVec  = SkyColorHelper::CalculateSkyColorWithFog(celestialAngle, perimeterElevation);

        // [FIX] Alpha should be FULLY OPAQUE (255) for both center and perimeter!
        // The gradient is achieved through RGB color interpolation, NOT alpha blending.
        // Previous bug: alpha=0 at edge caused the perimeter to be transparent/discarded.
        Rgba8 perimeterColor(
            static_cast<unsigned char>(perimeterColorVec.x * 255.0f),
            static_cast<unsigned char>(perimeterColorVec.y * 255.0f),
            static_cast<unsigned char>(perimeterColorVec.z * 255.0f),
            255 // [FIX] Fully opaque - gradient via RGB, not alpha!
        );
        perimeterColors.push_back(perimeterColor);
    }

    // ==========================================================================
    // [Step 3] Generate TRIANGLE_LIST with correct winding order
    // ==========================================================================
    for (size_t i = 0; i < perimeterPositions.size() - 1; ++i)
    {
        if (isUpperHemisphere)
        {
            // Upper hemisphere: CCW from below (player looks up)
            vertices.emplace_back(centerPosition, centerColor, centerUV, normal, tangent, bitangent);
            vertices.emplace_back(perimeterPositions[i + 1], perimeterColors[i + 1], perimeterUVs[i + 1], normal, tangent, bitangent);
            vertices.emplace_back(perimeterPositions[i], perimeterColors[i], perimeterUVs[i], normal, tangent, bitangent);
        }
        else
        {
            // Lower hemisphere: CCW from above (player looks down)
            vertices.emplace_back(centerPosition, centerColor, centerUV, normal, tangent, bitangent);
            vertices.emplace_back(perimeterPositions[i], perimeterColors[i], perimeterUVs[i], normal, tangent, bitangent);
            vertices.emplace_back(perimeterPositions[i + 1], perimeterColors[i + 1], perimeterUVs[i + 1], normal, tangent, bitangent);
        }
    }

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
std::vector<Vertex> SkyGeometryHelper::GenerateSunriseStrip(const Vec4& sunriseColor, float sunAngle)
{
    // ==========================================================================
    // [Sunrise/Sunset Strip] - CPU-side Transform using Mat44 (Matching Minecraft)
    // ==========================================================================
    //
    // [CRITICAL] Minecraft applies transforms on CPU, NOT in shader!
    // Reference: LevelRenderer.java:1527-1548
    //
    //   poseStack.pushPose();
    //   poseStack.mulPose(Axis.XP.rotationDegrees(90.0F));        // Step 1
    //   j = Mth.sin(sunAngle) < 0 ? 180.0F : 0.0F;
    //   poseStack.mulPose(Axis.ZP.rotationDegrees(j));            // Step 2 (flip)
    //   poseStack.mulPose(Axis.ZP.rotationDegrees(90.0F));        // Step 3
    //   Matrix4f matrix4f3 = poseStack.last().pose();
    //   bufferBuilder.addVertex(matrix4f3, 0.0F, 100.0F, 0.0F);   // CPU transform!
    //
    // [Original Geometry] (MC Y-up, before transform):
    //   Center: (0, 100, 0)
    //   Outer:  (sin*120, cos*120, -cos*40*alpha)
    //
    // [Transform Chain] XP(90) * ZP(flip) * ZP(90):
    //   Combined rotation places strip at horizon, facing sunrise/sunset direction
    //
    // ==========================================================================

    std::vector<Vertex> vertices;
    vertices.reserve(51); // 17 triangles * 3 vertices

    constexpr int   SEGMENTS    = 16;
    constexpr float CENTER_DIST = 100.0f;
    constexpr float OUTER_DIST  = 120.0f;
    constexpr float DEPTH_SCALE = 40.0f;
    constexpr float TWO_PI      = 6.28318530718f;

    // ==========================================================================
    // [STEP 1] Build combined rotation matrix using Mat44
    // ==========================================================================
    // Minecraft transform order (column-major, right multiply):
    //   final = XP(90) * ZP(flip) * ZP(90)
    //
    // Using Mat44::Append which is "multiply on right in column notation"
    // So we build: start with XP(90), append ZP(flip), append ZP(90)
    // ==========================================================================

    // Calculate flip angle based on sun position
    float sinSunAngle = std::sin(sunAngle * TWO_PI);
    float flipAngle   = (sinSunAngle < 0.0f) ? 180.0f : 0.0f;

    // [DEBUG] Print transform debug info
    DebuggerPrintf("[SunsetStrip] sunAngle=%.4f, sin(sunAngle)=%.4f, flipAngle=%.1f\n",
                   sunAngle, sinSunAngle, flipAngle);

    // ==========================================================================
    // Preparing the Geometry same look between Vertex In
    // ==========================================================================

    // Build the combined transform matrix
    Mat44 transformMC = Mat44::MakeYRotationDegrees(180);


    // ==========================================================================
    // [STEP 2] Generate original MC geometry (Y-up space, no transform yet)
    // ==========================================================================
    float depthOffset = DEPTH_SCALE * sunriseColor.w;

    // [DEBUG] Print geometry parameters
    DebuggerPrintf("[SunsetStrip] sunriseColor.w(alpha)=%.4f, depthOffset=%.2f\n",
                   sunriseColor.w, depthOffset);

    // Center vertex in MC space: (0, 100, 0)
    Vec3 centerMC(0.0f, CENTER_DIST, 0.0f);

    // Outer vertices in MC space: (sin*120, cos*120, -cos*40*alpha)
    std::vector<Vec3> outerMC;
    outerMC.reserve(SEGMENTS + 1);

    for (int i = 0; i <= SEGMENTS; ++i)
    {
        float angle = static_cast<float>(i) * TWO_PI / static_cast<float>(SEGMENTS);
        float sinA  = std::sin(angle);
        float cosA  = std::cos(angle);

        // Original MC geometry (Y-up): (sin*120, cos*120, -cos*40*alpha)
        float mcX = sinA * OUTER_DIST;
        float mcY = cosA * OUTER_DIST;
        float mcZ = -cosA * depthOffset;

        // [DEBUG] Print depth variation
        if (i == 0 || i == 8) // 打印首尾顶点
        {
            DebuggerPrintf("[SunsetStrip] Vertex[%d]: angle=%.1f, cosA=%.3f, mcZ=%.2f\n",
                           i, angle * 57.2958f, cosA, mcZ);
        }

        outerMC.emplace_back(mcX, mcY, mcZ);

        // [DEBUG] Print first vertex depth
        if (i == 0)
        {
            DebuggerPrintf("[SunsetStrip] First outer vertex: cos(0)=%.2f, mcZ=%.2f\n", cosA, mcZ);
        }
    }

    // ==========================================================================
    // [STEP 3] Apply transform using Mat44::TransformPosition3D
    // ==========================================================================
    Vec3 centerPosition = transformMC.TransformPosition3D(centerMC);

    std::vector<Vec3> outerPositions;
    outerPositions.reserve(SEGMENTS + 1);
    for (const Vec3& mcPos : outerMC)
    {
        outerPositions.push_back(transformMC.TransformPosition3D(mcPos));
    }

    // ==========================================================================
    // [STEP 4] Setup colors and vertex attributes
    // ==========================================================================
    // [FIX] Match Minecraft: pure white vertices, GPU applies sunriseColor
    // Reference: Minecraft VS In data shows (255,255,255,255) center, (255,255,255,0) edge
    Rgba8 centerColor = Rgba8(255, 255, 255, 255); // Pure white, fully opaque
    Rgba8 outerColor  = Rgba8(255, 255, 255, 0); // Pure white, fully transparent

    // Default vertex attributes
    Vec2 defaultUV(0.5f, 0.5f);
    Vec3 normal(0.0f, 0.0f, -1.0f);
    Vec3 tangent(1.0f, 0.0f, 0.0f);
    Vec3 bitangent(0.0f, 1.0f, 0.0f);

    // ==========================================================================
    // [STEP 5] Generate TRIANGLE_LIST (16 triangles)
    // ==========================================================================
    for (int i = 0; i < SEGMENTS; ++i)
    {
        vertices.emplace_back(centerPosition, centerColor, defaultUV, normal, tangent, bitangent);
        vertices.emplace_back(outerPositions[i], outerColor, defaultUV, normal, tangent, bitangent);
        vertices.emplace_back(outerPositions[i + 1], outerColor, defaultUV, normal, tangent, bitangent);
    }

    // ==========================================================================
    // Adjust the Geometry same out with vertex out
    // ==========================================================================
    /// [ADJUST]
    /// need adjust the geomrtry, sorry my brain is lazy
    Mat44 adjust = Mat44::MakeZRotationDegrees(90);
    adjust.AppendZRotation(flipAngle);
    TransformVertexArray3D(vertices, adjust);
    return vertices;
}
