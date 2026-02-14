#include "StarGeometryHelper.hpp"
#include "Engine/Math/RawNoise.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/MathUtils.hpp"
#include <cmath>

//-------------------------------------------------------------------------------------------
std::vector<Vertex> StarGeometryHelper::GenerateStarVertices(unsigned int seed)
{
    // Reference: Minecraft LevelRenderer.java:571-620 createStars()
    //
    // Minecraft Algorithm:
    // 1. Create 1500 stars with Random(seed=10842)
    // 2. For each star:
    //    a. Generate random position on unit sphere
    //    b. Calculate tangent vectors for billboard
    //    c. Generate quad vertices with random size
    //
    // Key differences from Minecraft:
    // - Uses SquirrelNoise5 instead of Java Random
    // - Coordinate system: Z-up instead of Y-up

    std::vector<Vertex> vertices;
    vertices.reserve(TOTAL_VERTICES);

    int noiseIndex = 0; // Incremented for each random value needed

    for (int starIndex = 0; starIndex < STAR_COUNT; ++starIndex)
    {
        // ==================== Step 1: Generate random position on unit sphere ====================
        // Reference: LevelRenderer.java:576-582
        //
        // Minecraft uses:
        //   double d0 = random.nextFloat() * 2.0F - 1.0F;  // X: [-1, 1]
        //   double d1 = random.nextFloat() * 2.0F - 1.0F;  // Y: [-1, 1]
        //   double d2 = random.nextFloat() * 2.0F - 1.0F;  // Z: [-1, 1]
        //   double d3 = 0.15F + random.nextFloat() * 0.1F; // Size: [0.15, 0.25]
        //   double d4 = d0*d0 + d1*d1 + d2*d2;             // Length squared
        //
        // Rejection sampling: skip if d4 >= 1.0 or d4 == 0.0

        float x, y, z;
        float lengthSquared;

        // Rejection sampling loop (same as Minecraft)
        do
        {
            // Generate random values in [-1, 1]
            x = Get1dNoiseZeroToOne(noiseIndex++, seed) * 2.0f - 1.0f;
            y = Get1dNoiseZeroToOne(noiseIndex++, seed) * 2.0f - 1.0f;
            z = Get1dNoiseZeroToOne(noiseIndex++, seed) * 2.0f - 1.0f;

            lengthSquared = x * x + y * y + z * z;
        }
        while (lengthSquared >= 1.0f || lengthSquared == 0.0f);

        // Generate star size: [0.15, 0.25]
        float starSize = STAR_SIZE_MIN + Get1dNoiseZeroToOne(noiseIndex++, seed) * 0.1f;

        // ==================== Step 2: Normalize and scale to sphere radius ====================
        // Reference: LevelRenderer.java:585-588
        //
        // Minecraft:
        //   double d5 = 1.0 / Math.sqrt(d4);
        //   d0 *= d5; d1 *= d5; d2 *= d5;  // Normalize
        //   d0 *= 100.0; d1 *= 100.0; d2 *= 100.0;  // Scale to radius

        float invLength = 1.0f / sqrtf(lengthSquared);
        x               *= invLength * STAR_RADIUS;
        y               *= invLength * STAR_RADIUS;
        z               *= invLength * STAR_RADIUS;

        // ==================== Step 3: Calculate tangent vectors for billboard ====================
        // Reference: LevelRenderer.java:589-600
        //
        // Minecraft calculates rotation angle from atan2(d2, d0) and atan2(sqrt(d0*d0+d2*d2), d1)
        // Then creates tangent vectors using sin/cos of these angles
        //
        // Coordinate conversion: MC(X,Y,Z) -> Engine(X,Z,Y)
        // So MC's d0,d1,d2 become our x,z,y

        // Convert to Engine coordinates for angle calculation
        // MC: d0=X, d1=Y(up), d2=Z(forward)
        // Engine: X=X, Y=forward, Z=up
        // So: engineX = mcX, engineY = mcZ, engineZ = mcY

        float mcX = x; // Same as engine X
        float mcY = z; // Engine Z = MC Y (up axis)
        float mcZ = y; // Engine Y = MC Z (forward axis)

        // Calculate angles (Minecraft algorithm)
        // d6 = atan2(d2, d0) = atan2(mcZ, mcX)
        // d7 = sin(d6), d8 = cos(d6)
        float angleXZ = atan2f(mcZ, mcX);
        float sinXZ   = sinf(angleXZ);
        float cosXZ   = cosf(angleXZ);

        // d9 = atan2(sqrt(d0*d0 + d2*d2), d1)
        // d10 = sin(d9), d11 = cos(d9)
        float horizontalDist = sqrtf(mcX * mcX + mcZ * mcZ);
        float angleY         = atan2f(horizontalDist, mcY);
        float sinY           = sinf(angleY);
        float cosY           = cosf(angleY);

        // ==================== Step 4: Generate quad vertices ====================
        // Reference: LevelRenderer.java:601-618
        //
        // Minecraft generates 4 corners with offsets:
        //   corner 0: (-1, -1) -> (l, m) = (-starSize, -starSize)
        //   corner 1: (-1, +1) -> (l, m) = (-starSize, +starSize)
        //   corner 2: (+1, +1) -> (l, m) = (+starSize, +starSize)
        //   corner 3: (+1, -1) -> (l, m) = (+starSize, -starSize)
        //
        // Vertex position formula (Minecraft):
        //   vx = d0 + l * d8 * d10 - m * d7
        //   vy = d1 + l * (-d11)
        //   vz = d2 + l * d7 * d10 + m * d8

        // Define corner offsets (CCW winding)
        const float cornerOffsets[4][2] = {
            {-1.0f, -1.0f}, // Corner 0: bottom-left
            {-1.0f, +1.0f}, // Corner 1: top-left
            {+1.0f, +1.0f}, // Corner 2: top-right
            {+1.0f, -1.0f} // Corner 3: bottom-right
        };

        // Calculate 4 corner positions
        Vec3 corners[4];
        for (int i = 0; i < 4; ++i)
        {
            float l = cornerOffsets[i][0] * starSize;
            float m = cornerOffsets[i][1] * starSize;

            // Minecraft formula (in MC coordinates)
            float vx_mc = mcX + l * cosXZ * sinY - m * sinXZ;
            float vy_mc = mcY + l * (-cosY);
            float vz_mc = mcZ + l * sinXZ * sinY + m * cosXZ;

            // Convert back to Engine coordinates
            // Engine: X=mcX, Y=mcZ, Z=mcY
            corners[i] = Vec3(vx_mc, vz_mc, vy_mc);
        }

        // Generate 2 triangles (6 vertices) for the quad
        // Triangle 1: corners 0, 1, 2
        // Triangle 2: corners 0, 2, 3
        const int triangleIndices[6] = {0, 1, 2, 0, 2, 3};

        for (int i = 0; i < 6; ++i)
        {
            Vertex vertex;
            vertex.m_position    = corners[triangleIndices[i]];
            vertex.m_color       = Rgba8::WHITE; // White color, brightness controlled by shader
            vertex.m_uvTexCoords = Vec2(0.0f, 0.0f); // No texture, just solid color

            vertices.push_back(vertex);
        }
    }

    return vertices;
}

//-------------------------------------------------------------------------------------------
float StarGeometryHelper::CalculateStarBrightness(float celestialAngle, float rainStrength)
{
    // Reference: Minecraft LevelRenderer.java:1556-1560
    //
    // Minecraft code:
    //   float f10 = level.getStarBrightness(partialTick) * f9;
    //   if (f10 > 0.0F) {
    //       FogRenderer.setupNoFog();
    //       this.starBuffer.bind();
    //       ...
    //   }
    //
    // Reference: Minecraft ClientLevel.java:251-260 getStarBrightness()
    //
    // Minecraft code:
    //   public float getStarBrightness(float partialTick) {
    //       float f = this.getTimeOfDay(partialTick);
    //       float f1 = 1.0F - (Mth.cos(f * ((float)Math.PI * 2F)) * 2.0F + 0.25F);
    //       f1 = Mth.clamp(f1, 0.0F, 1.0F);
    //       return f1 * f1 * 0.5F;
    //   }
    //
    // Note: celestialAngle here is equivalent to Minecraft's timeOfDay

    constexpr float TWO_PI = 6.28318530718f;

    // Calculate base star brightness from celestial angle
    float cosValue   = cosf(celestialAngle * TWO_PI);
    float brightness = 1.0f - (cosValue * 2.0f + 0.25f);

    // Clamp to [0, 1]
    brightness = GetClampedZeroToOne(brightness);

    // Quadratic falloff (Minecraft style)
    brightness = brightness * brightness * 0.5f;

    // Apply rain reduction
    // Reference: LevelRenderer.java:1556 - f9 is (1.0 - rainLevel)
    brightness *= (1.0f - rainStrength);

    return brightness;
}
