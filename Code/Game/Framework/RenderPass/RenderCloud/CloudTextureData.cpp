/**
 * @file CloudTextureData.cpp
 * @brief Sodium-style cloud texture data processing
 * @date 2025-12-02
 *
 * Responsibilities:
 * - Load and parse clouds.png (256x256) texture
 * - Pre-calculate face visibility masks for each cloud cell
 * - Support wrap-around texture sampling via Slice class
 * - Coordinate system: Minecraft (X,Z) -> Engine (Y,X)
 *
 * Reference: Sodium CloudRenderer.java Line 498-665
 */

#include "CloudTextureData.hpp"
#include "Engine/Core/Image.hpp"
#include "Engine/Core/Rgba8.hpp"
#include <algorithm>
#include <cmath>

// ========================================
// Static Helper Functions
// ========================================

/**
 * @brief Pack Rgba8 color to ARGB uint32_t format
 * @param color Source Rgba8 color
 * @return ARGB packed as 0xAARRGGBB
 */
static uint32_t PackARGB(const Rgba8& color)
{
    return (static_cast<uint32_t>(color.a) << 24) |
        (static_cast<uint32_t>(color.r) << 16) |
        (static_cast<uint32_t>(color.g) << 8) |
        static_cast<uint32_t>(color.b);
}

/**
 * @brief Floor modulo operation (handles negative numbers correctly)
 * @param a Dividend
 * @param b Divisor (must be positive)
 * @return Non-negative remainder in range [0, b)
 *
 * Example: FloorMod(-1, 256) = 255 (not -1)
 */
static int FloorMod(int a, int b)
{
    int r = a % b;
    return (r < 0) ? (r + b) : r;
}

// ========================================
// CloudTextureData Implementation
// ========================================

CloudTextureData::CloudTextureData(int width, int height)
    : m_width(width)
      , m_height(height)
{
    // Pre-allocate storage for faces and colors arrays
    int totalPixels = width * height;
    m_faces.resize(totalPixels, 0);
    m_colors.resize(totalPixels, 0);
}

std::unique_ptr<CloudTextureData> CloudTextureData::Load(const Image& image)
{
    // Validate texture dimensions (must be 256x256)
    IntVec2 dimensions = image.GetDimensions();
    if (dimensions.x != 256 || dimensions.y != 256)
    {
        return nullptr;
    }

    // Create CloudTextureData instance
    auto data = std::unique_ptr<CloudTextureData>(new CloudTextureData(dimensions.x, dimensions.y));

    // Load and process texture data
    if (!data->LoadTextureData(image))
    {
        return nullptr;
    }

    return data;
}

/**
 * @brief Load texture data and pre-calculate face masks
 * Reference: Sodium CloudRenderer.java Line 498-528
 *
 * Transparency detection supports two PNG formats:
 * 1. RGBA PNG: alpha < 10 indicates transparent
 * 2. Grayscale/indexed PNG: black (R<10) indicates transparent, white is cloud
 */
bool CloudTextureData::LoadTextureData(const Image& texture)
{
    int opaqueCount = 0;

    for (int z = 0; z < m_height; ++z)
    {
        for (int x = 0; x < m_width; ++x)
        {
            Rgba8 color = texture.GetTexelColor(IntVec2(x, z));

            // Transparency detection (supports RGBA and grayscale formats)
            bool isTransparent = (color.a < 10) ||
                (color.a == 255 && color.r < 10 && color.g < 10 && color.b < 10);

            if (isTransparent)
            {
                continue;
            }

            opaqueCount++;

            // Force white color for all clouds (ignore potential edge artifacts)
            uint32_t argb = PackARGB(Rgba8(255, 255, 255, 255));

            int index       = GetCellIndex(x, z, m_width);
            m_colors[index] = argb;
            m_faces[index]  = static_cast<uint8_t>(GetOpenFaces(texture, argb, x, z));
        }
    }

    return opaqueCount > 0;
}

/**
 * @brief Create a texture slice with wrap-around sampling
 * Reference: Sodium CloudRenderer.java Line 570-590
 *
 * @param originX Center X coordinate in texture space
 * @param originZ Center Z coordinate in texture space
 * @param radius Slice radius (result is (2*radius+1) x (2*radius+1))
 * @return Slice containing faces and colors data
 */
CloudTextureData::Slice CloudTextureData::CreateSlice(int originX, int originZ, int radius) const
{
    int sliceWidth  = 2 * radius + 1;
    int sliceHeight = 2 * radius + 1;

    Slice slice(sliceWidth, sliceHeight, radius);

    // Copy row by row with wrap-around handling
    for (int dstZ = 0; dstZ < sliceHeight; ++dstZ)
    {
        // Source texture Z coordinate (with wrap-around)
        int srcZ = FloorMod(originZ - radius + dstZ, m_height);

        // Copy columns with X-axis wrap-around
        int srcX = FloorMod(originX - radius, m_width);
        for (int dstX = 0; dstX < sliceWidth;)
        {
            // Calculate copy length (avoid crossing texture boundary)
            int length = std::min(m_width - srcX, sliceWidth - dstX);

            // Calculate source and destination indices
            int srcPos = GetCellIndex(srcX, srcZ, m_width);
            int dstPos = GetCellIndex(dstX, dstZ, sliceWidth);

            // Copy faces[] and colors[] arrays
            std::copy_n(m_faces.begin() + srcPos, length, slice.m_faces.begin() + dstPos);
            std::copy_n(m_colors.begin() + srcPos, length, slice.m_colors.begin() + dstPos);

            // Update position (wrap to start after boundary)
            dstX += length;
            srcX = 0;
        }
    }

    return slice;
}

/**
 * @brief Pre-calculate visible faces based on 4 horizontal neighbors
 * Reference: Sodium CloudRenderer.java Line 620-643
 *
 * Logic: Render a face when neighbor color differs from self (including transparent neighbors)
 * Top and bottom faces are always visible (single-layer cloud)
 *
 * Coordinate mapping (Minecraft -> Engine):
 * - West (-X) -> -Y, East (+X) -> +Y
 * - North (-Z) -> -X, South (+Z) -> +X
 */
int CloudTextureData::GetOpenFaces(const Image& image, uint32_t color, int x, int z)
{
    // Top and bottom faces always visible
    int faces = FACE_MASK_NEG_Z | FACE_MASK_POS_Z;

    uint32_t neighbor;

    // Left neighbor (Minecraft West -> Engine -Y)
    neighbor = GetNeighborTexel(image, x - 1, z);
    if (color != neighbor)
    {
        faces |= FACE_MASK_NEG_Y;
    }

    // Right neighbor (Minecraft East -> Engine +Y)
    neighbor = GetNeighborTexel(image, x + 1, z);
    if (color != neighbor)
    {
        faces |= FACE_MASK_POS_Y;
    }

    // Back neighbor (Minecraft North -> Engine -X)
    neighbor = GetNeighborTexel(image, x, z - 1);
    if (color != neighbor)
    {
        faces |= FACE_MASK_NEG_X;
    }

    // Front neighbor (Minecraft South -> Engine +X)
    neighbor = GetNeighborTexel(image, x, z + 1);
    if (color != neighbor)
    {
        faces |= FACE_MASK_POS_X;
    }

    return faces;
}

/**
 * @brief Get neighbor texel color with wrap-around sampling
 * @return 0 for transparent, 0xFFFFFFFF (white) for opaque
 *
 * Uses same transparency detection as LoadTextureData for consistency
 */
uint32_t CloudTextureData::GetNeighborTexel(const Image& image, int x, int z)
{
    IntVec2 dimensions = image.GetDimensions();

    // Wrap coordinates
    int wrappedX = FloorMod(x, dimensions.x);
    int wrappedZ = FloorMod(z, dimensions.y);

    // Get color
    Rgba8 color = image.GetTexelColor(IntVec2(wrappedX, wrappedZ));

    // Transparency detection (same logic as LoadTextureData)
    bool isTransparent = (color.a < 10) ||
        (color.a == 255 && color.r < 10 && color.g < 10 && color.b < 10);

    // Return 0 for transparent (matches m_colors[] default)
    // Return white for opaque (matches LoadTextureData storage)
    if (isTransparent)
    {
        return 0;
    }
    return PackARGB(Rgba8(255, 255, 255, 255));
}

bool CloudTextureData::IsTransparent(uint32_t argb)
{
    // Check if alpha < 10 (Sodium threshold)
    uint8_t alpha = (argb >> 24) & 0xFF;
    return alpha < 10;
}

int CloudTextureData::GetCellIndex(int x, int z, int width)
{
    // Row-major 1D index
    return z * width + x;
}

// ========================================
// CloudTextureData::Slice Implementation
// ========================================

CloudTextureData::Slice::Slice(int width, int height, int radius)
    : m_width(width)
      , m_height(height)
      , m_radius(radius)
{
    // Pre-allocate storage
    int totalPixels = width * height;
    m_faces.resize(totalPixels, 0);
    m_colors.resize(totalPixels, 0);
}

int CloudTextureData::Slice::GetCellIndex(int x, int z) const
{
    // Convert relative coordinates (-radius to +radius) to array indices (0 to 2*radius)
    int arrayX = x + m_radius;
    int arrayZ = z + m_radius;
    return CloudTextureData::GetCellIndex(arrayX, arrayZ, m_width);
}

int CloudTextureData::Slice::GetCellFaces(int index) const
{
    return m_faces[index];
}

uint32_t CloudTextureData::Slice::GetCellColor(int index) const
{
    return m_colors[index];
}
