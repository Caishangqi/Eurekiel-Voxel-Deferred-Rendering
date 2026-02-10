#include "FlatWorldGenerator.hpp"

#include "Engine/Registry/Block/BlockRegistry.hpp"
#include "Engine/Voxel/Chunk/Chunk.hpp"

using namespace enigma::registry::block;

FlatWorldGenerator::FlatWorldGenerator()
    : TerrainGenerator("flat_world", "simpleminer")
{
}

void FlatWorldGenerator::InitializeBlockCache()
{
    if (m_initialized)
        return;

    m_stoneId = BlockRegistry::GetBlockId("simpleminer", "stone");
    m_grassId = BlockRegistry::GetBlockId("simpleminer", "grass");
    m_airId   = BlockRegistry::GetBlockId("simpleminer", "air");

    m_initialized = true;
}

bool FlatWorldGenerator::GenerateChunk(Chunk* chunk, int32_t chunkX, int32_t chunkY, uint32_t worldSeed)
{
    (void)worldSeed;

    if (!chunk)
        return false;

    InitializeBlockCache();

    GenerateTerrainShape(chunk, chunkX, chunkY);
    ApplySurfaceRules(chunk, chunkX, chunkY);
    // No features for flat world
    return true;
}

bool FlatWorldGenerator::GenerateTerrainShape(Chunk* chunk, int32_t chunkX, int32_t chunkY)
{
    (void)chunkX;
    (void)chunkY;

    if (m_stoneId < 0)
        return false;

    auto  stoneBlock = BlockRegistry::GetBlockById(m_stoneId);
    auto* stoneState = stoneBlock ? stoneBlock->GetDefaultState() : nullptr;

    if (!stoneState)
        return false;

    // Fill Z = 0 ~ 39 with stone across the entire chunk
    for (int z = 0; z < STONE_LAYERS; ++z)
    {
        for (int y = 0; y < Chunk::CHUNK_SIZE_Y; ++y)
        {
            for (int x = 0; x < Chunk::CHUNK_SIZE_X; ++x)
            {
                chunk->SetBlock(x, y, z, stoneState);
            }
        }
    }

    return true;
}

bool FlatWorldGenerator::ApplySurfaceRules(Chunk* chunk, int32_t chunkX, int32_t chunkY)
{
    (void)chunkX;
    (void)chunkY;

    if (m_grassId < 0)
        return false;

    auto  grassBlock = BlockRegistry::GetBlockById(m_grassId);
    auto* grassState = grassBlock ? grassBlock->GetDefaultState() : nullptr;

    if (!grassState)
        return false;

    // Place grass at Z = 40 (on top of stone)
    for (int y = 0; y < Chunk::CHUNK_SIZE_Y; ++y)
    {
        for (int x = 0; x < Chunk::CHUNK_SIZE_X; ++x)
        {
            chunk->SetBlock(x, y, GROUND_HEIGHT, grassState);
        }
    }

    return true;
}

bool FlatWorldGenerator::GenerateFeatures(Chunk* chunk, int32_t chunkX, int32_t chunkY)
{
    // Flat world has no features (no trees, ores, etc.)
    (void)chunk;
    (void)chunkX;
    (void)chunkY;
    return true;
}

std::string FlatWorldGenerator::GetConfigDescription() const
{
    return "Flat World Generator: 40 stone + 1 grass (ground at Z=40)";
}

int FlatWorldGenerator::GetGroundHeightAt(int globalX, int globalY) const
{
    (void)globalX;
    (void)globalY;
    return GROUND_HEIGHT;
}
