#include "SimpleMinerTreeGenerator.hpp"
#include "SimpleMinerGenerator.hpp"
#include "../TreeStamps/OakTreeStamp.hpp"
#include "../TreeStamps/OakSnowTreeStamp.hpp"
#include "../TreeStamps/BirchTreeStamp.hpp"
#include "../TreeStamps/SpruceTreeStamp.hpp"
#include "../TreeStamps/SpruceSnowTreeStamp.hpp"
#include "../TreeStamps/JungleTreeStamp.hpp"
#include "../TreeStamps/AcaciaTreeStamp.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Voxel/Chunk/Chunk.hpp"
#include "Engine/Registry/Block/BlockRegistry.hpp"
#include "Engine/Math/IntVec3.hpp"
#include "Engine/Voxel/Biome/Biome.hpp"
#include <algorithm>

SimpleMinerTreeGenerator::SimpleMinerTreeGenerator(uint32_t                    worldSeed, const TerrainGenerator* terrainGenerator,
                                                   const SimpleMinerGenerator* simpleMinerGenerator)
    : TreeGenerator(worldSeed, terrainGenerator)
      , m_simpleMinerGenerator(simpleMinerGenerator)
{
    InitializeStampCache();
}

void SimpleMinerTreeGenerator::InitializeStampCache()
{
    // Initialize all tree stamp types
    m_stampCache["oak"]         = std::make_shared<OakTreeStamp>();
    m_stampCache["oak_snow"]    = std::make_shared<OakSnowTreeStamp>();
    m_stampCache["birch"]       = std::make_shared<BirchTreeStamp>();
    m_stampCache["spruce"]      = std::make_shared<SpruceTreeStamp>();
    m_stampCache["spruce_snow"] = std::make_shared<SpruceSnowTreeStamp>();
    m_stampCache["jungle"]      = std::make_shared<JungleTreeStamp>();
    m_stampCache["acacia"]      = std::make_shared<AcaciaTreeStamp>();
    m_stampCache["cactus"]      = std::make_shared<CactusStamp>();

    LogInfo("TreeGenerator", "Initialized %zu tree stamp types", m_stampCache.size());
}

std::shared_ptr<TreeStamp> SimpleMinerTreeGenerator::GetTreeStamp(const std::string& typeName) const
{
    auto it = m_stampCache.find(typeName);
    if (it != m_stampCache.end())
    {
        return it->second;
    }
    return nullptr;
}

std::string SimpleMinerTreeGenerator::SelectTreeType(const enigma::voxel::Biome* biome, int globalX, int globalY) const
{
    if (!biome)
    {
        return "oak"; // Default fallback
    }

    std::string biomeName = biome->GetName();

    // Desert biome -> Cactus (70%) or Acacia trees (30%)
    if (biomeName.find("desert") != std::string::npos)
    {
        // Use rotation noise to determine cactus vs acacia
        float random = SampleTreeRotationNoise(globalX, globalY);
        return (random < 0.7f) ? "cactus" : "acacia";
    }
    // Jungle biome -> Jungle trees
    else if (biomeName.find("jungle") != std::string::npos)
    {
        return "jungle";
    }
    // Taiga biomes -> Spruce trees (with snow variant for cold regions)
    else if (biomeName.find("taiga") != std::string::npos)
    {
        // Check if this is a snowy taiga biome
        if (biomeName.find("snowy") != std::string::npos)
        {
            return "spruce_snow";
        }

        // For regular taiga in T0 (very cold) regions, also use spruce_snow
        // T0 category: temperature < -0.45
        if (m_simpleMinerGenerator)
        {
            auto biomeAt = m_simpleMinerGenerator->GetBiomeAt(globalX, globalY);
            if (biomeAt)
            {
                float temperature = biomeAt->GetClimateSettings().temperature;
                if (temperature < -0.45f)
                {
                    return "spruce_snow";
                }
            }
        }

        return "spruce";
    }
    // Forest biome -> Oak or Birch (random based on rotation noise)
    else if (biomeName.find("forest") != std::string::npos)
    {
        float random = SampleTreeRotationNoise(globalX, globalY);
        return (random > 0.5f) ? "oak" : "birch";
    }
    // Plains biome -> Oak or Birch (with snowy variant for cold regions)
    else if (biomeName.find("plains") != std::string::npos)
    {
        // Check if this is a snowy plains biome
        if (biomeName.find("snowy") != std::string::npos)
        {
            return "oak_snow"; // Snowy plains -> Oak with snow
        }
        float random = SampleTreeRotationNoise(globalX, globalY);
        return (random > 0.5f) ? "oak" : "birch";
    }
    // Default -> Oak
    else
    {
        return "oak";
    }
}

std::string SimpleMinerTreeGenerator::SelectTreeSize(float noiseValue) const
{
    if (noiseValue >= 0.95f)
    {
        return "large";
    }
    else if (noiseValue >= 0.85f)
    {
        return "medium";
    }
    else
    {
        return "small";
    }
}

float SimpleMinerTreeGenerator::GetTreeThreshold(const enigma::voxel::Biome* biome) const
{
    if (!biome)
    {
        return 0.92f; // Default threshold (increased from 0.85)
    }

    std::string biomeName = biome->GetName();

    // Forest biome -> Lower threshold (more trees)
    if (biomeName.find("forest") != std::string::npos)
    {
        return 0.82f; // Increased from 0.7 to reduce density
    }
    // Plains biome -> Very high threshold (almost no trees)
    else if (biomeName.find("plains") != std::string::npos)
    {
        return 0.998f; // Increased from 0.95 to make plains nearly treeless
    }
    // Desert biome -> Extremely high threshold (almost no trees)
    else if (biomeName.find("desert") != std::string::npos)
    {
        return 0.999f; // Increased from 0.98 for even fewer trees
    }
    // Taiga biomes -> Medium-high threshold
    else if (biomeName.find("taiga") != std::string::npos)
    {
        return 0.88f; // New: specific threshold for taiga biomes
    }
    // Jungle biomes -> Lower threshold (dense trees)
    else if (biomeName.find("jungle") != std::string::npos)
    {
        return 0.78f; // New: specific threshold for jungle biomes (high density)
    }
    // Savanna biomes -> High threshold (sparse trees)
    else if (biomeName.find("savanna") != std::string::npos)
    {
        return 0.94f; // New: specific threshold for savanna biomes
    }
    // Default threshold (for other biomes)
    else
    {
        return 1.0; // Increased from 0.85 to reduce overall density
    }
}

std::shared_ptr<TreeStamp> SimpleMinerTreeGenerator::GetOrCreateStamp(const std::string& treeType, const std::string& treeSize)
{
    // Create cache key: "type_size" (e.g., "oak_small")
    std::string cacheKey = treeType + "_" + treeSize;

    // Check if stamp is already in cache
    auto it = m_stampCache.find(cacheKey);
    if (it != m_stampCache.end())
    {
        return it->second;
    }

    // Create new stamp using factory methods
    std::shared_ptr<TreeStamp> newStamp = nullptr;

    // Convert size to lowercase for comparison
    std::string sizeLower = treeSize;
    std::transform(sizeLower.begin(), sizeLower.end(), sizeLower.begin(), [](unsigned char c) -> char
    {
        return static_cast<char>(std::tolower(c));
    });

    // Create stamp based on type and size
    if (treeType == "oak")
    {
        if (sizeLower == "small")
            newStamp = std::make_shared<OakTreeStamp>(OakTreeStamp::CreateSmall());
        else if (sizeLower == "medium")
            newStamp = std::make_shared<OakTreeStamp>(OakTreeStamp::CreateMedium());
        else if (sizeLower == "large")
            newStamp = std::make_shared<OakTreeStamp>(OakTreeStamp::CreateLarge());
    }
    else if (treeType == "oak_snow")
    {
        if (sizeLower == "small")
            newStamp = std::make_shared<OakSnowTreeStamp>(OakSnowTreeStamp::CreateSmall());
        else if (sizeLower == "medium")
            newStamp = std::make_shared<OakSnowTreeStamp>(OakSnowTreeStamp::CreateMedium());
        else if (sizeLower == "large")
            newStamp = std::make_shared<OakSnowTreeStamp>(OakSnowTreeStamp::CreateLarge());
    }
    else if (treeType == "birch")
    {
        if (sizeLower == "small")
            newStamp = std::make_shared<BirchTreeStamp>(BirchTreeStamp::CreateSmall());
        else if (sizeLower == "medium")
            newStamp = std::make_shared<BirchTreeStamp>(BirchTreeStamp::CreateMedium());
        else if (sizeLower == "large")
            newStamp = std::make_shared<BirchTreeStamp>(BirchTreeStamp::CreateLarge());
    }
    else if (treeType == "spruce")
    {
        if (sizeLower == "small")
            newStamp = std::make_shared<SpruceTreeStamp>(SpruceTreeStamp::CreateSmall());
        else if (sizeLower == "medium")
            newStamp = std::make_shared<SpruceTreeStamp>(SpruceTreeStamp::CreateMedium());
        else if (sizeLower == "large")
            newStamp = std::make_shared<SpruceTreeStamp>(SpruceTreeStamp::CreateLarge());
    }
    else if (treeType == "spruce_snow")
    {
        if (sizeLower == "small")
            newStamp = std::make_shared<SpruceSnowTreeStamp>(SpruceSnowTreeStamp::CreateSmall());
        else if (sizeLower == "medium")
            newStamp = std::make_shared<SpruceSnowTreeStamp>(SpruceSnowTreeStamp::CreateMedium());
        else if (sizeLower == "large")
            newStamp = std::make_shared<SpruceSnowTreeStamp>(SpruceSnowTreeStamp::CreateLarge());
    }
    else if (treeType == "jungle")
    {
        if (sizeLower == "small")
            newStamp = std::make_shared<JungleTreeStamp>(JungleTreeStamp::CreateSmall());
        else if (sizeLower == "medium")
            newStamp = std::make_shared<JungleTreeStamp>(JungleTreeStamp::CreateMedium());
        else if (sizeLower == "large")
            newStamp = std::make_shared<JungleTreeStamp>(JungleTreeStamp::CreateLarge());
    }
    else if (treeType == "acacia")
    {
        if (sizeLower == "small")
            newStamp = std::make_shared<AcaciaTreeStamp>(AcaciaTreeStamp::CreateSmall());
        else if (sizeLower == "medium")
            newStamp = std::make_shared<AcaciaTreeStamp>(AcaciaTreeStamp::CreateMedium());
        else if (sizeLower == "large")
            newStamp = std::make_shared<AcaciaTreeStamp>(AcaciaTreeStamp::CreateLarge());
    }
    else if (treeType == "cactus")
    {
        if (sizeLower == "small")
            newStamp = std::make_shared<CactusStamp>(CactusStamp::CreateSmall());
        else if (sizeLower == "medium")
            newStamp = std::make_shared<CactusStamp>(CactusStamp::CreateMedium());
        else if (sizeLower == "large")
            newStamp = std::make_shared<CactusStamp>(CactusStamp::CreateLarge());
    }

    // If stamp was created, add to cache
    if (newStamp)
    {
        m_stampCache[cacheKey] = newStamp;
        LogDebug("TreeGenerator", "Created and cached tree stamp: %s", cacheKey.c_str());
    }
    else
    {
        LogWarn("TreeGenerator", "Failed to create tree stamp for type=%s, size=%s", treeType.c_str(), treeSize.c_str());
    }

    return newStamp;
}

std::string SimpleMinerTreeGenerator::DetermineTreeType(int globalX, int globalY) const
{
    // Get biome at this position using SimpleMinerGenerator
    if (m_simpleMinerGenerator)
    {
        auto biome = m_simpleMinerGenerator->GetBiomeAt(globalX, globalY);
        if (biome)
        {
            return SelectTreeType(biome.get(), globalX, globalY);
        }
    }

    // Fallback: use noise-based selection if biome is not available
    float sizeNoise = SampleTreeSizeNoise(globalX, globalY);

    // Simple distribution based on noise value
    if (sizeNoise < 0.2f)
        return "oak";
    else if (sizeNoise < 0.4f)
        return "birch";
    else if (sizeNoise < 0.6f)
        return "spruce";
    else if (sizeNoise < 0.8f)
        return "jungle";
    else
        return "acacia";
}

bool SimpleMinerTreeGenerator::CanPlaceTree(int globalX, int globalY, int groundHeight, int treeHeight) const
{
    UNUSED(globalY)
    UNUSED(globalY)
    UNUSED(globalX)
    // Check if ground height is valid
    if (groundHeight < 0 || groundHeight >= Chunk::CHUNK_SIZE_Z - treeHeight)
    {
        return false;
    }

    // ========== KEY FIX: Prevent trees from generating underwater ==========
    // SEA_LEVEL is defined in SimpleMinerGenerator.hpp as 64
    // If ground height is below sea level, the area is underwater
    // Trees should not generate in underwater areas
    constexpr int SEA_LEVEL = 64;
    if (groundHeight < SEA_LEVEL)
    {
        return false;
    }

    // TODO: Add more validation:
    // - Check surface block type (grass, dirt, etc.)
    // - Check if enough vertical space

    return true;
}

bool SimpleMinerTreeGenerator::PlaceTree(Chunk* chunk, int32_t chunkX, int32_t           chunkY,
                                         int    globalX, int   globalY, const TreeStamp& stamp)
{
    if (!chunk)
    {
        LogError("TreeGenerator", "PlaceTree - null chunk provided");
        return false;
    }

    // Get ground height at tree origin using terrain generator
    int groundZ = GetGroundHeightAt(globalX, globalY);

    // Validate ground height
    if (groundZ < 0 || groundZ >= Chunk::CHUNK_SIZE_Z - stamp.GetHeight())
    {
        return false;
    }

    // Get all blocks from the tree stamp
    const std::vector<TreeStampBlock>& blocks = stamp.GetBlocks();

    int blocksPlaced  = 0;
    int blocksSkipped = 0;

    // Iterate through all blocks in the tree stamp
    for (const auto& stampBlock : blocks)
    {
        // Calculate global position of this block
        IntVec3 globalPos(
            globalX + stampBlock.offset.x,
            globalY + stampBlock.offset.y,
            groundZ + stampBlock.offset.z
        );

        // Convert global position to local chunk coordinates
        int localX = globalPos.x - chunkX * Chunk::CHUNK_SIZE_X;
        int localY = globalPos.y - chunkY * Chunk::CHUNK_SIZE_Y;
        int localZ = globalPos.z;

        // ========== KEY: Cross-Chunk Boundary Check ==========
        // Only modify blocks within the current chunk
        // Cross-chunk parts are automatically skipped
        // This ensures thread safety and cross-chunk consistency
        // Following Professor's principle (conversation-0.txt:138):
        // "Chunk (5,6) can do all of the math and noise it needs to know 
        //  even its neighbor Chunk's noise and tree placement without ever 
        //  looking at or talking to the neighbor Chunk."
        if (localX < 0 || localX >= Chunk::CHUNK_SIZE_X ||
            localY < 0 || localY >= Chunk::CHUNK_SIZE_Y ||
            localZ < 0 || localZ >= Chunk::CHUNK_SIZE_Z)
        {
            // Block is outside current chunk, skip it
            // Neighbor chunks will place their own parts independently
            blocksSkipped++;
            continue;
        }

        // Get block state from registry
        // First get the Block from BlockRegistry, then get its default BlockState
        auto block = enigma::registry::block::BlockRegistry::GetBlockById(stampBlock.blockId);
        if (!block)
        {
            LogWarn("TreeGenerator", "Failed to get block for ID: %d", stampBlock.blockId);
            continue;
        }
        auto* blockState = block->GetDefaultState();
        if (!blockState)
        {
            LogWarn("TreeGenerator", "Failed to get block state for ID: %d", stampBlock.blockId);
            continue;
        }

        // Check if we should overwrite the existing block
        auto* existingBlock = chunk->GetBlock(localX, localY, localZ);
        if (existingBlock)
        {
            int existingBlockId = existingBlock->GetBlock()->GetNumericId();
            UNUSED(existingBlockId)
            // Don't overwrite solid blocks (stone, ores, etc.)
            // Only overwrite air, grass, leaves, and other replaceable blocks
            // TODO: Use block properties to determine if block is replaceable
            // For now, use a simple heuristic based on block ID

            // Get block name to check if it's replaceable
            std::string blockName = existingBlock->GetBlock()->GetRegistryName();

            // Replaceable blocks: air, grass, leaves, water (for now)
            bool isReplaceable = (blockName == "air" ||
                blockName.find("grass") != std::string::npos ||
                blockName.find("leaves") != std::string::npos ||
                blockName == "water");

            if (!isReplaceable)
            {
                // Don't overwrite solid blocks
                blocksSkipped++;
                continue;
            }
        }

        // Place the block in the chunk
        chunk->SetBlock(localX, localY, localZ, blockState);
        blocksPlaced++;
    }

    LogDebug("TreeGenerator", "Placed tree at (%d, %d, %d): %d blocks placed, %d blocks skipped",
             globalX, globalY, groundZ, blocksPlaced, blocksSkipped);

    return blocksPlaced > 0;
}

bool SimpleMinerTreeGenerator::GenerateTrees(Chunk* chunk, int32_t chunkX, int32_t chunkY)
{
    if (!chunk)
    {
        LogError("TreeGenerator", "GenerateTrees - null chunk provided");
        return false;
    }

    // Clear noise cache for this chunk
    ClearNoiseCache();

    // Calculate expanded chunk boundaries
    int expandedMinX, expandedMaxX, expandedMinY, expandedMaxY;
    CalculateExpandedBounds(chunkX, chunkY, expandedMinX, expandedMaxX, expandedMinY, expandedMaxY);

    int treesPlaced = 0;

    // Statistics for logging (tree type and size distribution)
    std::unordered_map<std::string, int> treeTypeCount;
    std::unordered_map<std::string, int> treeSizeCount;

    // Iterate through expanded area
    for (int globalX = expandedMinX; globalX < expandedMaxX; globalX++)
    {
        for (int globalY = expandedMinY; globalY < expandedMaxY; globalY++)
        {
            // Sample tree noise at this position
            float treeNoise = SampleTreeNoise(globalX, globalY);

            // Check if this is a local maximum
            if (!IsLocalMaximum(globalX, globalY, treeNoise))
            {
                continue;
            }

            // Get biome at this position to determine tree threshold
            float treeThreshold = 0.7f; // Default threshold
            if (m_simpleMinerGenerator)
            {
                auto biome = m_simpleMinerGenerator->GetBiomeAt(globalX, globalY);
                if (biome)
                {
                    treeThreshold = GetTreeThreshold(biome.get());
                }
            }

            // Check if noise value is above biome-specific threshold
            if (treeNoise < treeThreshold)
            {
                continue;
            }

            // Get ground height at this position
            int groundHeight = GetGroundHeightAt(globalX, globalY);

            // Determine tree type based on biome
            std::string treeType = DetermineTreeType(globalX, globalY);

            // Select tree size based on noise value
            std::string treeSize = SelectTreeSize(treeNoise);

            // Get or create tree stamp with type and size
            auto treeStamp = GetOrCreateStamp(treeType, treeSize);
            if (!treeStamp)
            {
                LogWarn("TreeGenerator", "Failed to get tree stamp for type=%s, size=%s", treeType.c_str(), treeSize.c_str());
                continue;
            }

            // Get tree height from stamp
            int treeHeight = treeStamp->GetHeight();

            // Check if tree can be placed
            if (!CanPlaceTree(globalX, globalY, groundHeight, treeHeight))
            {
                continue;
            }

            // Place tree using TreeStamp
            if (PlaceTree(chunk, chunkX, chunkY, globalX, globalY, *treeStamp))
            {
                treesPlaced++;

                // Update statistics
                treeTypeCount[treeType]++;
                treeSizeCount[treeSize]++;
            }
        }
    }

    // Log tree generation summary with type and size distribution
    LogDebug("TreeGenerator", "Generated %d trees for chunk (%d, %d)", treesPlaced, chunkX, chunkY);

    // Log tree type distribution
    if (!treeTypeCount.empty())
    {
        std::string typeDistribution = "Tree types: ";
        for (const auto& pair : treeTypeCount)
        {
            typeDistribution += pair.first + "=" + std::to_string(pair.second) + " ";
        }
        LogDebug("TreeGenerator", "%s", typeDistribution.c_str());
    }

    // Log tree size distribution
    if (!treeSizeCount.empty())
    {
        std::string sizeDistribution = "Tree sizes: ";
        for (const auto& pair : treeSizeCount)
        {
            sizeDistribution += pair.first + "=" + std::to_string(pair.second) + " ";
        }
        LogDebug("TreeGenerator", "%s", sizeDistribution.c_str());
    }

    return true;
}
