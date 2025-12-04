/**
 * @file CloudTextureData.hpp
 * @brief Sodium-style cloud texture data management
 * @date 2025-12-02
 *
 * Responsibilities:
 * - Load and parse clouds.png (256x256) texture
 * - Pre-calculate face visibility masks for each cloud cell
 * - Support wrap-around texture sampling via Slice class
 *
 * Reference: Sodium CloudRenderer.java Line 557-665
 */

#pragma once
#include <vector>
#include <memory>
#include <cstdint>

class Image;

/**
 * @class CloudTextureData
 * @brief Stores cloud texture data with pre-calculated face visibility masks
 *
 * Data stored per pixel:
 * - m_faces[]: 6-bit face visibility mask (which faces are exposed)
 * - m_colors[]: ARGB color value (always white 0xFFFFFFFF for clouds)
 */
class CloudTextureData
{
public:
    /**
     * @class Slice
     * @brief Texture slice with wrap-around sampling support
     * Reference: Sodium CloudRenderer.java Line 667-693
     */
    class Slice
    {
    public:
        Slice(int width, int height, int radius);

        /// Convert 2D coordinates to 1D array index
        int GetCellIndex(int x, int z) const;

        /// Get 6-bit face visibility mask for cell
        int GetCellFaces(int index) const;

        /// Get ARGB color for cell
        uint32_t GetCellColor(int index) const;

    private:
        friend class CloudTextureData;

        int                   m_width;
        int                   m_height;
        int                   m_radius;
        std::vector<uint8_t>  m_faces; ///< Face visibility masks
        std::vector<uint32_t> m_colors; ///< ARGB colors
    };

    /// Factory method: Load 256x256 texture from Image
    static std::unique_ptr<CloudTextureData> Load(const Image& image);

    /// Create texture slice with wrap-around sampling
    Slice CreateSlice(int originX, int originZ, int radius) const;

    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }

private:
    CloudTextureData(int width, int height);

    /// Load texture data and pre-calculate face masks
    bool LoadTextureData(const Image& texture);

    /// Calculate visible faces based on 4 horizontal neighbors
    static int GetOpenFaces(const Image& image, uint32_t color, int x, int z);

    /// Get neighbor texel color with wrap-around
    static uint32_t GetNeighborTexel(const Image& image, int x, int z);

    /// Check if color is transparent (alpha < 10)
    static bool IsTransparent(uint32_t argb);

    /// Convert 2D coordinates to 1D array index
    static int GetCellIndex(int x, int z, int width);

    int                   m_width;
    int                   m_height;
    std::vector<uint8_t>  m_faces; ///< Per-pixel face visibility masks (6 bits)
    std::vector<uint32_t> m_colors; ///< Per-pixel ARGB colors
};

// ========================================
// Face Mask Constants
// ========================================
// Reference: Sodium CloudRenderer.java Line 47-54
// [FIX] Match Sodium's bit values exactly, then map to our coordinate system
//
// Sodium (Minecraft coords, Y=height):
//   NEG_Y=1 (bottom), POS_Y=2 (top), NEG_X=4, POS_X=8, NEG_Z=16, POS_Z=32
//
// Our engine (Z=height):
//   Minecraft Y -> Our Z (height)
//   Minecraft X -> Our X (horizontal)
//   Minecraft Z -> Our Y (horizontal)

constexpr int FACE_MASK_NEG_Z = 1; ///< Bottom face (Sodium NEG_Y=1)
constexpr int FACE_MASK_POS_Z = 2; ///< Top face (Sodium POS_Y=2)
constexpr int FACE_MASK_NEG_X = 4; ///< -X face (Sodium NEG_X=4)
constexpr int FACE_MASK_POS_X = 8; ///< +X face (Sodium POS_X=8)
constexpr int FACE_MASK_NEG_Y = 16; ///< -Y face (Sodium NEG_Z=16)
constexpr int FACE_MASK_POS_Y = 32; ///< +Y face (Sodium POS_Z=32)
