#include "Game/Gameplay/TreeStamps/OakSnowTreeStamp.hpp"
#include "Engine/Registry/Block/BlockRegistry.hpp"

//-----------------------------------------------------------------------------------------------
// Constructor
//
OakSnowTreeStamp::OakSnowTreeStamp(const std::vector<TreeStampBlock>& blocks, const std::string& sizeName)
    : TreeStamp(blocks)
      , m_sizeName(sizeName)
{
    // Initialize Block IDs from BlockRegistry
    InitializeBlockIds(GetLogBlockName(), GetLeavesBlockName());
}

//-----------------------------------------------------------------------------------------------
// Create Small Oak Snow Tree
//
// Coordinate Conversion from Minecraft to SimpleMiner:
// - Minecraft: X=East, Y=Up, Z=North
// - SimpleMiner: X=Forward, Y=Left, Z=Up
// - Conversion: SM.X = MC.X, SM.Y = MC.Z, SM.Z = MC.Y
//
using namespace enigma::registry::block;

OakSnowTreeStamp OakSnowTreeStamp::CreateSmall()
{
    // Small Oak: 5 blocks tall, 3x3 leaves crown (same structure as OakTreeStamp, but with snow leaves)
    // Query Block IDs from registry (not hardcoded)
    int logId    = BlockRegistry::GetBlockId("simpleminer", "oak_log");
    int leavesId = BlockRegistry::GetBlockId("simpleminer", "oak_leaves_snow");

    std::vector<TreeStampBlock> blocks;

    // Trunk (4 blocks tall)
    blocks.push_back(TreeStampBlock(IntVec3(0, 0, 0), logId)); // Base
    blocks.push_back(TreeStampBlock(IntVec3(0, 0, 1), logId));
    blocks.push_back(TreeStampBlock(IntVec3(0, 0, 2), logId));
    blocks.push_back(TreeStampBlock(IntVec3(0, 0, 3), logId));

    // Leaves layer 1 (Z=3, 5x5 cross pattern)
    blocks.push_back(TreeStampBlock(IntVec3(-2, 0, 3), leavesId));
    blocks.push_back(TreeStampBlock(IntVec3(-1, 0, 3), leavesId));
    blocks.push_back(TreeStampBlock(IntVec3(1, 0, 3), leavesId));
    blocks.push_back(TreeStampBlock(IntVec3(2, 0, 3), leavesId));
    blocks.push_back(TreeStampBlock(IntVec3(0, -2, 3), leavesId));
    blocks.push_back(TreeStampBlock(IntVec3(0, -1, 3), leavesId));
    blocks.push_back(TreeStampBlock(IntVec3(0, 1, 3), leavesId));
    blocks.push_back(TreeStampBlock(IntVec3(0, 2, 3), leavesId));

    // Leaves layer 2 (Z=4, 3x3 full)
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            blocks.push_back(TreeStampBlock(IntVec3(x, y, 4), leavesId));
        }
    }

    // Top leaves (Z=5, single block)
    blocks.push_back(TreeStampBlock(IntVec3(0, 0, 5), leavesId));

    return OakSnowTreeStamp(blocks, "Small");
}

//-----------------------------------------------------------------------------------------------
// Create Medium Oak Snow Tree
//
OakSnowTreeStamp OakSnowTreeStamp::CreateMedium()
{
    // Medium Oak: 6 blocks tall, larger crown (same structure as OakTreeStamp, but with snow leaves)
    int logId    = BlockRegistry::GetBlockId("simpleminer", "oak_log");
    int leavesId = BlockRegistry::GetBlockId("simpleminer", "oak_leaves_snow");

    std::vector<TreeStampBlock> blocks;

    // Trunk (5 blocks tall)
    for (int z = 0; z <= 4; ++z)
    {
        blocks.push_back(TreeStampBlock(IntVec3(0, 0, z), logId));
    }

    // Leaves layer 1 (Z=4, 5x5 full)
    for (int x = -2; x <= 2; ++x)
    {
        for (int y = -2; y <= 2; ++y)
        {
            if (x == 0 && y == 0) continue; // Skip trunk
            blocks.push_back(TreeStampBlock(IntVec3(x, y, 4), leavesId));
        }
    }

    // Leaves layer 2 (Z=5, 5x5 full)
    for (int x = -2; x <= 2; ++x)
    {
        for (int y = -2; y <= 2; ++y)
        {
            blocks.push_back(TreeStampBlock(IntVec3(x, y, 5), leavesId));
        }
    }

    // Leaves layer 3 (Z=6, 3x3 full)
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            blocks.push_back(TreeStampBlock(IntVec3(x, y, 6), leavesId));
        }
    }

    return OakSnowTreeStamp(blocks, "Medium");
}

//-----------------------------------------------------------------------------------------------
// Create Large Oak Snow Tree
//
OakSnowTreeStamp OakSnowTreeStamp::CreateLarge()
{
    // Large Oak: 7 blocks tall, large crown (same structure as OakTreeStamp, but with snow leaves)
    int logId    = BlockRegistry::GetBlockId("simpleminer", "oak_log");
    int leavesId = BlockRegistry::GetBlockId("simpleminer", "oak_leaves_snow");

    std::vector<TreeStampBlock> blocks;

    // Trunk (6 blocks tall)
    for (int z = 0; z <= 5; ++z)
    {
        blocks.push_back(TreeStampBlock(IntVec3(0, 0, z), logId));
    }

    // Leaves layer 1 (Z=4, 7x7 diamond pattern)
    for (int x = -3; x <= 3; ++x)
    {
        for (int y = -3; y <= 3; ++y)
        {
            int dist = abs(x) + abs(y);
            if (dist <= 3 && !(x == 0 && y == 0))
            {
                blocks.push_back(TreeStampBlock(IntVec3(x, y, 4), leavesId));
            }
        }
    }

    // Leaves layer 2 (Z=5, 5x5 full)
    for (int x = -2; x <= 2; ++x)
    {
        for (int y = -2; y <= 2; ++y)
        {
            if (x == 0 && y == 0) continue; // Skip trunk
            blocks.push_back(TreeStampBlock(IntVec3(x, y, 5), leavesId));
        }
    }

    // Leaves layer 3 (Z=6, 5x5 full)
    for (int x = -2; x <= 2; ++x)
    {
        for (int y = -2; y <= 2; ++y)
        {
            blocks.push_back(TreeStampBlock(IntVec3(x, y, 6), leavesId));
        }
    }

    // Leaves layer 4 (Z=7, 3x3 full)
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            blocks.push_back(TreeStampBlock(IntVec3(x, y, 7), leavesId));
        }
    }

    return OakSnowTreeStamp(blocks, "Large");
}
