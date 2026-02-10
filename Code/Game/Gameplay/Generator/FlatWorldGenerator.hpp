#pragma once
#include "Engine/Voxel/Generation/TerrainGenerator.hpp"

using namespace enigma::voxel;

/**
 * @brief Superflat world generator
 *
 * Generates a flat world with configurable layers.
 * Default: 40 layers of stone (Z=0~39) + 1 layer of grass (Z=40).
 * Coordinate system: +X forward, +Y left, +Z up.
 */
class FlatWorldGenerator : public TerrainGenerator
{
public:
    explicit FlatWorldGenerator();
    ~FlatWorldGenerator() override = default;

    bool        GenerateChunk(Chunk* chunk, int32_t chunkX, int32_t chunkY, uint32_t worldSeed = 0) override;
    bool        GenerateTerrainShape(Chunk* chunk, int32_t chunkX, int32_t chunkY) override;
    bool        ApplySurfaceRules(Chunk* chunk, int32_t chunkX, int32_t chunkY) override;
    bool        GenerateFeatures(Chunk* chunk, int32_t chunkX, int32_t chunkY) override;
    std::string GetConfigDescription() const override;
    int         GetGroundHeightAt(int globalX, int globalY) const override;

    int32_t GetSeaLevel() const override { return 0; }
    int32_t GetBaseHeight() const override { return GROUND_HEIGHT; }

private:
    void InitializeBlockCache();

    // Layer configuration
    static constexpr int STONE_LAYERS  = 64; // Z = 0 ~ 39
    static constexpr int GROUND_HEIGHT = 64; // Z = 40 (grass layer)

    // Cached block IDs
    int m_stoneId = -1;
    int m_grassId = -1;
    int m_airId   = -1;

    bool m_initialized = false;
};
