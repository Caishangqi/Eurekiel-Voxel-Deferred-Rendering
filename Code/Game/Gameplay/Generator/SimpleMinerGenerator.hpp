#pragma once
#include "Engine/Voxel/Generation/TerrainGenerator.hpp"
#include "Engine/Voxel/NoiseGenerator/PerlinNoiseGenerator.hpp"
#include "Engine/Voxel/Function/SplineDensityFunction.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Core/Engine.hpp"
#include <unordered_map>
#include <memory>

#include "Engine/Core/LogCategory/LogCategory.hpp"

DECLARE_LOG_CATEGORY_EXTERN(LogWorldGenerator)

namespace enigma::voxel
{
    class Biome;
}

namespace enigma::registry::block
{
    class Block;
}

using namespace enigma::voxel;

/**
 * @brief SimpleMiner world generator implementation
 *
 * Implements the Assignment 02 chunk generation algorithm using Perlin noise
 * for terrain, humidity, temperature, and other biome characteristics.
 * Based on the provided pseudocode from Assignment 02 specification.
 */
class SimpleMinerGenerator : public TerrainGenerator
{
private:
    // ========== Phase 2-4: Noise Parameters (1:1 Professor's Final Version) ==========
    // Source: Course Blog "Ship It" - Oct 21, 2025

    // Temperature Noise (2D)
    static constexpr float        TEMPERATURE_NOISE_SCALE   = 512.0f; // 教授最终版本
    static constexpr unsigned int TEMPERATURE_NOISE_OCTAVES = 2; // 教授最终版本

    // Humidity Noise (2D)
    static constexpr float        HUMIDITY_NOISE_SCALE   = 512.0f; // 教授最终版本
    static constexpr unsigned int HUMIDITY_NOISE_OCTAVES = 4; // 教授最终版本

    // Continentalness Noise (2D)
    static constexpr float        CONTINENTAL_NOISE_SCALE   = 1024.0f; // 教授最终版本 (扩大 Biome 区域)
    static constexpr unsigned int CONTINENTAL_NOISE_OCTAVES = 4; // 教授最终版本 (增加细节)

    // Erosion Noise (2D)
    static constexpr float        EROSION_NOISE_SCALE   = 512.0f; // 教授最终版本 (扩大地形特征)
    static constexpr unsigned int EROSION_NOISE_OCTAVES = 8; // 教授最终版本 (增强起伏)

    // PeaksValleys Noise (2D)
    static constexpr float        PEAKS_VALLEYS_NOISE_SCALE   = 512.0f; // 教授最终版本
    static constexpr unsigned int PEAKS_VALLEYS_NOISE_OCTAVES = 8; // 教授最终版本 (丰富峰谷细节)

    // 3D Density Noise
    static constexpr float        DENSITY_NOISE_SCALE   = 64.0f; // 教授最终版本
    static constexpr unsigned int DENSITY_NOISE_OCTAVES = 8; // 教授最终版本

    // Terrain Generation Constants
    static constexpr float TERRAIN_BASE_HEIGHT = 64.0f; // 基准高度 (海平面)
    static constexpr float BIAS_PER_Z          = 0.015f; // 每个Z单位的密度偏置
    static constexpr int   SEA_LEVEL           = 64; // 海平面高度

    // ========== Noise Type Enumeration ==========
    enum class NoiseType : unsigned int
    {
        Temperature = 0,
        Humidity = 1,
        Continentalness = 2,
        Erosion = 3,
        Weirdness = 4,
        PeaksValleys = 5
    };

    // ========== Climate Parameter Categories ==========

    // Continentalness Categories (6 levels)
    enum class ContinentalnessCategory
    {
        DEEP_OCEAN, // c < -0.3
        OCEAN, // -0.3 <= c < 0.0
        COAST, // 0.0 <= c < 0.1
        NEAR_INLAND, // 0.1 <= c < 0.2
        MID_INLAND, // 0.2 <= c < 0.4
        FAR_INLAND // c >= 0.4
    };

    // Temperature Categories (5 levels)
    enum class TemperatureCategory
    {
        T0, // t < -0.45 (Frozen)
        T1, // -0.45 <= t < -0.15 (Cold)
        T2, // -0.15 <= t < 0.20 (Normal)
        T3, // 0.20 <= t < 0.55 (Warm)
        T4 // t >= 0.55 (Hot)
    };

    // Humidity Categories (5 levels)
    enum class HumidityCategory
    {
        H0, // h < -0.35 (Arid)
        H1, // -0.35 <= h < -0.10 (Dry)
        H2, // -0.10 <= h < 0.10 (Normal)
        H3, // 0.10 <= h < 0.30 (Humid)
        H4 // h >= 0.30 (Wet)
    };

    // PeaksValleys Categories (5 levels)
    enum class PeaksValleysCategory
    {
        VALLEYS, // pv < -0.85
        LOW, // -0.85 <= pv < -0.2
        MID, // -0.2 <= pv < 0.2
        HIGH, // 0.2 <= pv < 0.85
        PEAKS // pv >= 0.85
    };

    // Erosion Categories (7 levels)
    enum class ErosionCategory
    {
        E0, // e < -0.78
        E1, // -0.78 <= e < -0.375
        E2, // -0.375 <= e < -0.2225
        E3, // -0.2225 <= e < 0.05
        E4, // 0.05 <= e < 0.45
        E5, // 0.45 <= e < 0.55
        E6 // e >= 0.55
    };

    // ========== Member Variables ==========

    // World seed
    uint32_t m_worldSeed;

    // Phase 2-4: Spline Density Functions
    std::shared_ptr<SplineDensityFunction> m_heightOffsetSpline; // Height offset based on continentalness
    std::shared_ptr<SplineDensityFunction> m_squashingSpline; // Squashing factor based on continentalness
    std::shared_ptr<SplineDensityFunction> m_erosionSpline; // Erosion influence
    std::shared_ptr<SplineDensityFunction> m_peaksValleysSpline; // Peaks/Valleys influence

    // Noise Generators
    std::unique_ptr<enigma::voxel::PerlinNoiseGenerator> m_temperatureNoise;
    std::unique_ptr<enigma::voxel::PerlinNoiseGenerator> m_humidityNoise;
    std::unique_ptr<enigma::voxel::PerlinNoiseGenerator> m_continentalnessNoise;
    std::unique_ptr<enigma::voxel::PerlinNoiseGenerator> m_erosionNoise;
    std::unique_ptr<enigma::voxel::PerlinNoiseGenerator> m_weirdnessNoise;
    std::unique_ptr<enigma::voxel::PerlinNoiseGenerator> m_peaksValleysNoise;
    std::unique_ptr<enigma::voxel::PerlinNoiseGenerator> m_densityNoise3D;

    // Block ID Cache (for thread-safe access)
    std::unordered_map<std::string, int>                                     m_blockIdCache;
    std::unordered_map<int, std::shared_ptr<enigma::registry::block::Block>> m_blockByIdCache;

    // Common Block IDs (cached for performance)
    int m_airId        = -1;
    int m_grassId      = -1;
    int m_dirtId       = -1;
    int m_stoneId      = -1;
    int m_sandId       = -1;
    int m_waterId      = -1;
    int m_iceId        = -1;
    int m_lavaId       = -1;
    int m_obsidianId   = -1;
    int m_coalOreId    = -1;
    int m_ironOreId    = -1;
    int m_goldOreId    = -1;
    int m_diamondOreId = -1;

    // Phase 5: Additional Block IDs
    int m_gravelId    = -1;
    int m_clayId      = -1;
    int m_sandstoneId = -1;
    int m_snowBlockId = -1;
    int m_andesiteId  = -1;
    int m_graniteId   = -1;
    int m_calciteId   = -1;
    int m_packedIceId = -1;
    int m_blueIceId   = -1;

    // Phase 6: Biome-specific grass variants
    int m_grassJungleId  = -1;
    int m_grassSavannaId = -1;
    int m_grassSnowId    = -1;
    int m_grassTaigaId   = -1;

    // Phase 6: Biome Instances
    std::shared_ptr<enigma::voxel::Biome> m_oceanBiome;
    std::shared_ptr<enigma::voxel::Biome> m_deepOceanBiome;
    std::shared_ptr<enigma::voxel::Biome> m_frozenOceanBiome;
    std::shared_ptr<enigma::voxel::Biome> m_beachBiome;
    std::shared_ptr<enigma::voxel::Biome> m_snowyBeachBiome;
    std::shared_ptr<enigma::voxel::Biome> m_desertBiome;
    std::shared_ptr<enigma::voxel::Biome> m_savannaBiome;
    std::shared_ptr<enigma::voxel::Biome> m_plainsBiome;
    std::shared_ptr<enigma::voxel::Biome> m_snowyPlainsBiome;
    std::shared_ptr<enigma::voxel::Biome> m_forestBiome;
    std::shared_ptr<enigma::voxel::Biome> m_jungleBiome;
    std::shared_ptr<enigma::voxel::Biome> m_taigaBiome;
    std::shared_ptr<enigma::voxel::Biome> m_snowyTaigaBiome;
    std::shared_ptr<enigma::voxel::Biome> m_stonyPeaksBiome;
    std::shared_ptr<enigma::voxel::Biome> m_snowyPeaksBiome;

    // Tree Block IDs (cached for performance)
    mutable int m_oakLogId           = -1;
    mutable int m_oakLeavesId        = -1;
    mutable int m_birchLogId         = -1;
    mutable int m_birchLeavesId      = -1;
    mutable int m_spruceLogId        = -1;
    mutable int m_spruceLeavesId     = -1;
    mutable int m_spruceLeavesSnowId = -1;
    mutable int m_jungleLogId        = -1;
    mutable int m_jungleLeavesId     = -1;
    mutable int m_acaciaLogId        = -1;
    mutable int m_acaciaLeavesId     = -1;

    // ========== Private Helper Methods ==========

    /**
     * @brief Initialize all noise generators with world seed
     */
    void InitializeNoiseGenerators();

    /**
     * @brief Initialize block cache for thread-safe access
     */
    void InitializeBlockCache();

    /**
     * @brief Initialize all biome instances
     */
    void InitializeBiomes();

    /**
     * @brief Classify continentalness value into category
     */
    ContinentalnessCategory ClassifyContinentalness(float c) const;

    /**
     * @brief Classify temperature value into category
     */
    TemperatureCategory ClassifyTemperature(float t) const;

    /**
     * @brief Classify humidity value into category
     */
    HumidityCategory ClassifyHumidity(float h) const;

    /**
     * @brief Classify peaks/valleys value into category
     */
    PeaksValleysCategory ClassifyPeaksValleys(float pv) const;

    /**
     * @brief Classify erosion value into category
     */
    ErosionCategory ClassifyErosion(float e) const;

    /**
     * @brief Sample 2D noise for specific type
     * @param globalX World X coordinate
     * @param globalZ World Z coordinate (Y in Minecraft terms)
     * @param type Noise type to sample
     * @return Noise value in range [-1, 1]
     */
    float SampleNoise2D(int globalX, int globalZ, NoiseType type) const;

    /**
     * @brief Sample continentalness noise at position
     */
    float SampleContinentalness(int globalX, int globalY) const;

    /**
     * @brief Sample erosion noise at position
     */
    float SampleErosion(int globalX, int globalY) const;

    /**
     * @brief Sample peaks/valleys noise at position
     */
    float SamplePeaksValleys(int globalX, int globalY) const;

    /**
     * @brief Evaluate height offset from continentalness using spline
     */
    float EvaluateHeightOffset(float continentalness) const;

    /**
     * @brief Evaluate squashing factor from continentalness using spline
     */
    float EvaluateSquashing(float continentalness) const;

    /**
     * @brief Evaluate erosion influence using spline
     */
    float EvaluateErosion(float erosion) const;

    /**
     * @brief Sample 3D density noise
     * @param globalX World X coordinate
     * @param globalY World Y coordinate (Z in Minecraft terms)
     * @param globalZ World Z coordinate (height)
     * @return Density value
     */
    float SampleNoise3D(int globalX, int globalY, int globalZ) const;

    /**
     * @brief Calculate final terrain density at specific position
     *
     * Reuses existing terrain shaping functions to compute accurate density.
     * This ensures GetGroundHeightAt() returns correct ground heights.
     *
     * @param globalX World X coordinate
     * @param globalY World Y coordinate
     * @param globalZ World Z coordinate (height)
     * @return Final density value (< 0.0f = solid, >= 0.0f = air)
     */
    float CalculateFinalDensity(int globalX, int globalY, int globalZ) const;

    /**
     * @brief Get cached block by name
     */
    std::shared_ptr<enigma::registry::block::Block> GetCachedBlock(const std::string& blockName) const;

    /**
     * @brief Get cached block by ID
     */
    std::shared_ptr<enigma::registry::block::Block> GetCachedBlockById(int blockId) const;

    /**
     * @brief Compute 2D Perlin noise (legacy compatibility)
     */
    float ComputePerlin2D(float        x, float           y, float          scale, unsigned int octaves,
                          float        persistence, float octaveScale, bool wrap,
                          unsigned int seed) const;

    /**
     * @brief Map value from input range to output range
     */
    float RangeMap(float value, float inMin, float inMax, float outMin, float outMax) const;

    /**
     * @brief Map value from input range to output range with clamping
     */
    float RangeMapClamped(float value, float inMin, float inMax, float outMin, float outMax) const;

    /**
     * @brief Linear interpolation
     */
    float Lerp(float a, float b, float t) const;

    /**
     * @brief Smooth step function (3rd order)
     */
    float SmoothStep3(float t) const;

    /**
     * @brief Phase 2: Generate terrain shape using 3D density
     */
    bool GenerateTerrainShape(Chunk* chunk, int32_t chunkX, int32_t chunkY) override;

    /**
     * @brief Phase 5: Apply surface rules (grass, dirt, sand, etc.)
     */
    bool ApplySurfaceRules(Chunk* chunk, int32_t chunkX, int32_t chunkY) override;

    /**
     * @brief Phase 7-9: Generate features (ores, caves, trees)
     */
    bool GenerateFeatures(Chunk* chunk, int32_t chunkX, int32_t chunkY) override;

    /**
     * @brief Compute 2D Perlin noise using engine's noise system
     */
    float Compute2dPerlinNoise(float        x, float           y, float          scale, unsigned int octaves,
                               float        persistence, float octaveScale, bool renormalize,
                               unsigned int seed) const;

public:
    /**
     * @brief Constructor
     * @param worldSeed Seed for world generation
     */
    explicit SimpleMinerGenerator(uint32_t worldSeed = 0);

    /**
     * @brief Destructor
     */
    ~SimpleMinerGenerator() override = default;

    /**
     * @brief Generate chunk terrain
     * @param chunk Chunk to generate
     * @param chunkX Chunk X coordinate
     * @param chunkY Chunk Y coordinate (Z in Minecraft terms)
     * @param worldSeed World seed (optional, uses constructor seed if 0)
     * @return true if generation succeeded
     */
    bool GenerateChunk(Chunk* chunk, int32_t chunkX, int32_t chunkY, uint32_t worldSeed = 0) override;

    /**
     * @brief Get generator configuration description
     * @return String describing this generator's configuration
     */
    std::string GetConfigDescription() const override;

    /**
     * @brief Get ground height at specific world position using noise calculation
     * 
     * This method calculates the ground height at a given (x, y) position by sampling
     * the 3D density noise without accessing chunk data. It uses binary search to find
     * the highest solid block (density < 0.0f) efficiently.
     * 
     * Thread-safe: Does not access chunk data, only uses noise calculations.
     * 
     * @param globalX World X coordinate
     * @param globalY World Y coordinate (Z in Minecraft terms)
     * @return Z coordinate of the highest solid block, or SEA_LEVEL if no solid block found
     */
    int GetGroundHeightAt(int globalX, int globalY) const override;

    /**
     * @brief Get biome at specific world position
     * 
     * This method is used by SimpleMinerTreeGenerator to determine tree types
     * based on biome characteristics. Made public to allow tree generator access.
     * 
     * Thread-safe: Only uses noise calculations, no chunk data access.
     * 
     * @param globalX World X coordinate
     * @param globalY World Y coordinate (Z in Minecraft terms)
     * @return Biome instance for this location
     */
    std::shared_ptr<enigma::voxel::Biome> GetBiomeAt(int globalX, int globalY) const;
};
