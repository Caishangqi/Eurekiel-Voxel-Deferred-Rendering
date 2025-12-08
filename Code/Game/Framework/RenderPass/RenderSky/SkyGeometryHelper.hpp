#pragma once
#include "Engine/Core/Vertex_PCU.hpp"
#include <vector>

#include "Engine/Graphic/Core/EnigmaGraphicCommon.hpp"

class AABB2;
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
    SkyGeometryHelper()                                    = delete;
    SkyGeometryHelper(const SkyGeometryHelper&)            = delete;
    SkyGeometryHelper& operator=(const SkyGeometryHelper&) = delete;

    //-----------------------------------------------------------------------------------------------
    /**
     * @brief Generate a sky disc mesh (TRIANGLE_FAN topology) with uniform color
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

    /**
     * @brief Generate a sky disc mesh with CPU-side fog blending (Iris-style)
     *
     * Creates sky dome vertices with per-vertex colors blended based on elevation angle.
     * This implements Iris-style CPU fog calculation where skyColor smoothly transitions
     * to fogColor at the horizon.
     *
     * @param centerZ Z-coordinate of the center vertex (typically +16.0 for sky dome)
     * @param celestialAngle Current celestial angle (0.0 - 1.0) from TimeOfDayManager
     * @return std::vector<Vertex> Vertex array with fog-blended colors
     *
     * @note Center vertex (zenith): uses pure skyColor
     * @note Perimeter vertices (horizon): uses pure fogColor
     * @note GPU interpolation creates smooth gradient between them
     *
     * @note Reference: Iris FogMode.OFF for SKY_BASIC - skyColor pre-blended with fog
     * @note Reference: Minecraft ClientLevel.getSkyColor() considers view direction
     */
    static std::vector<Vertex> GenerateSkyDiscWithFog(float centerZ, float celestialAngle);

    /**
     * @brief Generate a celestial billboard mesh (sun/moon) with CPU-calculated UV
     * 
     * Creates a 6-vertex quad (2 triangles) for rendering sun or moon as a billboard.
     * Billboard vertices are generated with offset positions that will be transformed
     * by the vertex shader to face the camera.
     * 
     * @param celestialType 0.0f for Sun, 1.0f for Moon (passed to vertex shader)
     * @param uvBounds UV bounds from SpriteAtlas::GetSprite(index).GetUVBounds()
     * @param color Vertex color (default: white)
     * @return std::vector<Vertex> Vertex array ready for TRIANGLE_LIST rendering (6 vertices)
     * 
     * @note Vertex count: 6 (2 triangles forming a quad)
     * @note UV calculation: CPU-side using SpriteAtlas (follows design.md line 156)
     * @note Position.xy: Billboard offset (-0.5 to 0.5)
     * @note Position.z: Celestial type (0 = Sun, 1 = Moon)
     */
    static std::vector<Vertex> GenerateCelestialBillboard(
        float        celestialType,
        const AABB2& uvBounds,
        const Rgba8& color = Rgba8::WHITE
    );

    /**
     * @brief Generate sunrise/sunset strip mesh (TRIANGLE_LIST topology) with CPU transform
     * 
     * Creates a 48-vertex fan mesh (16 triangles) for rendering sunrise/sunset glow.
     * Center vertex has high alpha (orange), outer vertices have zero alpha (transparent).
     * GPU interpolation creates smooth gradient effect.
     * 
     * [CRITICAL] Matches Minecraft's CPU-side transform approach:
     *   poseStack.mulPose(Axis.XP.rotationDegrees(90.0F));
     *   poseStack.mulPose(Axis.ZP.rotationDegrees(flip));  // 0 or 180 based on sunAngle
     *   poseStack.mulPose(Axis.ZP.rotationDegrees(90.0F));
     *   bufferBuilder.addVertex(matrix4f3, ...);  // CPU transform applied here!
     * 
     * @param sunriseColor RGBA color from CalculateSunriseColor() (r, g, b, alpha)
     * @param sunAngle Current sun angle (0.0-1.0) for flip calculation
     * @return std::vector<Vertex> Vertex array ready for TRIANGLE_LIST rendering (48 vertices)
     * 
     * @note Vertex count: 16 triangles * 3 = 48 vertices
     * @note Transform: XP(90) * ZP(flip) * ZP(90) applied on CPU
     * @note Reference: Minecraft LevelRenderer.java:1527-1548
     */
    static std::vector<Vertex> GenerateSunriseStrip(const Vec4& sunriseColor, float sunAngle);
};
