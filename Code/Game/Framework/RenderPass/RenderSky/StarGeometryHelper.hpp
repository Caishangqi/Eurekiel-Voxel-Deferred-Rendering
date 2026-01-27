#pragma once
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Graphic/Core/EnigmaGraphicCommon.hpp"
#include <vector>

/**
 * @brief Static helper class for generating star geometry (Minecraft vanilla style)
 *
 * This class provides utility functions to generate star field geometry compatible with
 * Minecraft's vanilla sky rendering system. All methods are static and the class cannot
 * be instantiated.
 *
 * @note Coordinate System Conversion:
 *       - Minecraft: Y-up (X-right, Z-forward)
 *       - Engine:    Z-up (X-right, Y-forward)
 *       - Conversion: (MC_X, MC_Y, MC_Z) -> (Engine_X, Engine_Z, Engine_Y)
 *
 * @note Random Number Generation:
 *       Uses Engine's RawNoise.hpp (SquirrelNoise5) instead of Java Random.
 *       Star positions will differ from Minecraft but distribution is equivalent.
 *
 * @note Reference: Minecraft LevelRenderer.java:571-620 createStars()
 */
class StarGeometryHelper
{
public:
    // [IMPORTANT] Static-only class - no instantiation allowed
    StarGeometryHelper()                                     = delete;
    StarGeometryHelper(const StarGeometryHelper&)            = delete;
    StarGeometryHelper& operator=(const StarGeometryHelper&) = delete;

    //-------------------------------------------------------------------------------------------
    // Constants (Minecraft-style values)
    //-------------------------------------------------------------------------------------------

    static constexpr int          STAR_COUNT        = 1500; // LevelRenderer.java:573
    static constexpr unsigned int DEFAULT_SEED      = 10842; // LevelRenderer.java:572
    static constexpr float        STAR_RADIUS       = 100.0f; // LevelRenderer.java:583
    static constexpr float        STAR_SIZE_MIN     = 0.15f; // LevelRenderer.java:584
    static constexpr float        STAR_SIZE_MAX     = 0.25f; // 0.15 + 0.1 = 0.25
    static constexpr int          VERTICES_PER_STAR = 6; // 2 triangles per quad
    static constexpr int          TOTAL_VERTICES    = STAR_COUNT * VERTICES_PER_STAR; // 9000

    //-------------------------------------------------------------------------------------------
    /**
     * @brief Generate star field vertices (Minecraft-style algorithm)
     *
     * Creates 1500 star quads distributed on a sphere using Minecraft's algorithm:
     * 1. Use SquirrelNoise5 (RawNoise.hpp) with given seed
     * 2. For each star:
     *    - Generate random position on unit sphere (uniform distribution)
     *    - Calculate tangent vectors for billboard orientation
     *    - Generate 6 vertices forming 2 triangles (quad)
     *    - Apply random size (0.15 ~ 0.25)
     *
     * @param seed Random seed for deterministic generation (default: 10842)
     * @return std::vector<Vertex> Vertex array for TRIANGLE_LIST (9000 vertices)
     *
     * @note Uses Engine's Get1dNoiseZeroToOne() from RawNoise.hpp
     * @note Reference: Minecraft LevelRenderer.java:571-620 createStars()
     */
    static std::vector<Vertex> GenerateStarVertices(unsigned int seed = DEFAULT_SEED);

    //-------------------------------------------------------------------------------------------
    /**
     * @brief Calculate star brightness based on celestial angle
     *
     * Implements Minecraft's star visibility formula:
     * - Stars visible when rainLevel < 1.0 AND skyDarken > 0
     * - Brightness = skyDarken * (1.0 - rainLevel)
     * - skyDarken calculated from celestialAngle
     *
     * @param celestialAngle Current celestial angle (0.0 - 1.0)
     * @param rainStrength Rain intensity (0.0 - 1.0), default 0.0
     * @return float Star brightness multiplier (0.0 - 1.0)
     *
     * @note Reference: Minecraft LevelRenderer.java:1556-1560
     * @note Reference: Minecraft ClientLevel.java:251-260 getStarBrightness()
     */
    static float CalculateStarBrightness(float celestialAngle, float rainStrength = 0.0f);
};
