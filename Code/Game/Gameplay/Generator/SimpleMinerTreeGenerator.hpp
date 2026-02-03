#pragma once
#include "Engine/Voxel/Generation/TreeGenerator.hpp"
#include "../TreeStamps/CactusStamp.hpp"
#include <memory>
#include <unordered_map>
#include <string>

class TreeStamp;
using namespace enigma::voxel;

// Forward declarations
namespace enigma::voxel
{
    class Biome;
}

/**
 * @brief SimpleMiner tree generation implementation
 *
 * Implements tree generation for SimpleMiner using the base TreeGenerator
 * algorithms. Supports multiple tree types (Oak, Birch, Spruce, Jungle, Acacia)
 * with biome-specific placement rules.
 *
 * Based on Professor Squirrel's noise-based local maximum algorithm.
 */
// Forward declaration
class SimpleMinerGenerator;

class SimpleMinerTreeGenerator : public TreeGenerator
{
private:
    // Tree stamp cache for different tree types and sizes
    // Key format: "type_size" (e.g., "oak_small", "birch_medium")
    std::unordered_map<std::string, std::shared_ptr<TreeStamp>> m_stampCache;

    // Reference to SimpleMinerGenerator for biome queries
    const SimpleMinerGenerator* m_simpleMinerGenerator;

public:
    /**
     * @brief Constructor
     * @param worldSeed Seed for world generation
     * @param terrainGenerator Reference to terrain generator for ground height queries
     * @param simpleMinerGenerator Reference to SimpleMinerGenerator for biome queries
     */
    explicit SimpleMinerTreeGenerator(uint32_t                    worldSeed, const TerrainGenerator* terrainGenerator,
                                      const SimpleMinerGenerator* simpleMinerGenerator);

    /**
     * @brief Destructor
     */
    ~SimpleMinerTreeGenerator() override = default;

    /**
     * @brief Generate trees for a chunk
     * 
     * Implements the tree generation algorithm:
     * 1. Calculate expanded chunk boundaries
     * 2. Sample tree noise for each position in expanded area
     * 3. Check for local maximum
     * 4. Determine tree type based on biome
     * 5. Place tree using appropriate TreeStamp
     * 
     * @param chunk Chunk to generate trees in
     * @param chunkX Chunk X coordinate
     * @param chunkY Chunk Y coordinate (Z in Minecraft terms)
     * @return true if generation succeeded
     */
    bool GenerateTrees(Chunk* chunk, int32_t chunkX, int32_t chunkY) override;

private:
    /**
     * @brief Initialize tree stamp cache
     * 
     * Loads all tree stamp types (Oak, Birch, Spruce, Jungle, Acacia)
     * into the cache for fast access during generation.
     */
    void InitializeStampCache();

    /**
     * @brief Get tree stamp by type name
     *
     * @param typeName Tree type name (e.g., "oak", "birch")
     * @return Tree stamp instance, or nullptr if not found
     */
    std::shared_ptr<TreeStamp> GetTreeStamp(const std::string& typeName) const;

    /**
     * @brief Get or create tree stamp by type and size
     *
     * Retrieves a tree stamp from cache, or creates a new one if not found.
     * Uses factory methods (CreateSmall/CreateMedium/CreateLarge) to create stamps.
     *
     * @param treeType Tree type name (e.g., "oak", "birch", "spruce")
     * @param treeSize Tree size name (e.g., "small", "medium", "large")
     * @return Tree stamp instance, or nullptr if type is invalid
     */
    std::shared_ptr<TreeStamp> GetOrCreateStamp(const std::string& treeType, const std::string& treeSize);

    /**
     * @brief Determine tree type based on biome and position
     *
     * Uses biome information and noise values to select appropriate
     * tree type for the given position.
     *
     * @param globalX World X coordinate
     * @param globalY World Y coordinate (Z in Minecraft terms)
     * @return Tree type name (e.g., "oak", "birch")
     */
    std::string DetermineTreeType(int globalX, int globalY) const;

    /**
     * @brief Select tree type based on biome
     *
     * Maps biome to appropriate tree type:
     * - Desert -> Acacia
     * - Jungle -> Jungle
     * - Taiga/SnowyTaiga -> Spruce
     * - Forest -> Oak or Birch (random)
     * - Default -> Oak
     *
     * @param biome Biome instance
     * @param globalX World X coordinate (for random variation)
     * @param globalY World Y coordinate (for random variation)
     * @return Tree type name (e.g., "oak", "birch", "spruce")
     */
    std::string SelectTreeType(const enigma::voxel::Biome* biome, int globalX, int globalY) const;

    /**
     * @brief Select tree size based on noise value
     *
     * Maps noise value to tree size:
     * - >= 0.95 -> "large"
     * - >= 0.85 -> "medium"
     * - < 0.85 -> "small"
     *
     * @param noiseValue Noise value in range [0, 1]
     * @return Tree size name ("small", "medium", "large")
     */
    std::string SelectTreeSize(float noiseValue) const;

    /**
     * @brief Get tree density threshold for biome
     *
     * Returns the minimum noise value required for tree placement:
     * - Forest: 0.7 (more trees)
     * - Plains: 0.95 (sparse)
     * - Desert: 0.98 (very sparse)
     * - Default: 0.85
     *
     * @param biome Biome instance
     * @return Threshold value in range [0, 1]
     */
    float GetTreeThreshold(const enigma::voxel::Biome* biome) const;

    /**
     * @brief Check if tree can be placed at position
     *
     * Validates that:
     * - Ground height is valid
     * - Surface block is suitable (grass, dirt, etc.)
     * - Enough vertical space for tree
     *
     * @param globalX World X coordinate
     * @param globalY World Y coordinate (Z in Minecraft terms)
     * @param groundHeight Ground height at position
     * @param treeHeight Required tree height
     * @return true if tree can be placed
     */
    bool CanPlaceTree(int globalX, int globalY, int groundHeight, int treeHeight) const;

    /**
     * @brief Place a tree at the specified position
     *
     * Places all blocks from the tree stamp into the chunk, handling:
     * - Cross-chunk boundaries (skips blocks outside current chunk)
     * - Block replacement rules (only replaces air, grass, leaves)
     * - Block state management
     *
     * @param chunk Chunk to place tree in
     * @param chunkX Chunk X coordinate
     * @param chunkY Chunk Y coordinate (Z in Minecraft terms)
     * @param globalX World X coordinate of tree origin
     * @param globalY World Y coordinate of tree origin
     * @param stamp Tree stamp to place
     * @return true if at least one block was placed
     */
    bool PlaceTree(Chunk* chunk, int32_t chunkX, int32_t           chunkY,
                   int    globalX, int   globalY, const TreeStamp& stamp);
};
