#pragma once
#include "Engine/Core/Vertex_PCU.hpp"
#include <vector>

//-----------------------------------------------------------------------------------------------
/**
 * @brief Static helper class for generating sky geometry (Minecraft vanilla style)
 * 
 * This class provides utility functions to generate sky disc geometry compatible with
 * Minecraft's vanilla sky rendering system. All methods are static and the class cannot
 * be instantiated.
 * 
 * @note Coordinate System Conversion:
 *       - Minecraft: Y-up (X-right, Z-forward)
 *       - Engine:    Z-up (X-right, Y-forward)
 *       - Conversion: (MC_X, MC_Y, MC_Z) -> (Engine_X, Engine_Z, Engine_Y)
 */
class SkyGeometryHelper
{
public:
    // [REMOVED] Static-only class - no instantiation allowed
    SkyGeometryHelper() = delete;
    SkyGeometryHelper(const SkyGeometryHelper&) = delete;
    SkyGeometryHelper& operator=(const SkyGeometryHelper&) = delete;

    //-----------------------------------------------------------------------------------------------
    /**
     * @brief Generate a sky disc mesh (TRIANGLE_FAN topology)
     * 
     * Creates a 10-vertex disc mesh following Minecraft vanilla specifications:
     * - 1 center vertex at (0, 0, centerZ)
     * - 9 perimeter vertices from -180° to +180° with 45° steps
     * - Radius: 512.0 units
     * - Topology: TRIANGLE_FAN (center vertex first, then perimeter vertices in CCW order)
     * 
     * @param centerZ Z-coordinate of the center vertex (typically ±16.0 for sky/void)
     * @param color Vertex color (default: white)
     * @return std::vector<Vertex> Vertex array ready for TRIANGLE_FAN rendering
     * 
     * @note Vertex count: 10 (1 center + 9 perimeter)
     * @note Perimeter vertices: angles [-180°, -135°, -90°, -45°, 0°, 45°, 90°, 135°, 180°]
     * @note Reference: Minecraft LevelRenderer.java:571-582 buildSkyDisc()
     */
    static std::vector<Vertex> GenerateSkyDisc(float centerZ, const Rgba8& color = Rgba8::WHITE);
};
