#include "Game/Gameplay/TreeStamps/BirchTreeStamp.hpp"
#include "Engine/Registry/Block/BlockRegistry.hpp"
using namespace enigma::registry::block;
//-----------------------------------------------------------------------------------------------
// Constructor
//
BirchTreeStamp::BirchTreeStamp(const std::vector<TreeStampBlock>& blocks, const std::string& sizeName)
    : TreeStamp(blocks)
      , m_sizeName(sizeName)
{
    // Initialize Block IDs from BlockRegistry
    InitializeBlockIds(GetLogBlockName(), GetLeavesBlockName());
}

//-----------------------------------------------------------------------------------------------
// Create Small Birch Tree
//
BirchTreeStamp BirchTreeStamp::CreateSmall()
{
    int logId    = BlockRegistry::GetBlockId("simpleminer", "birch_log");
    int leavesId = BlockRegistry::GetBlockId("simpleminer", "birch_leaves");

    std::vector<TreeStampBlock> blocks;

    // Trunk (5 blocks tall)
    for (int z = 0; z <= 4; ++z)
    {
        blocks.push_back(TreeStampBlock(IntVec3(0, 0, z), logId));
    }

    // Leaves (Z=4-5, 3x3)
    for (int z = 4; z <= 5; ++z)
    {
        for (int x = -1; x <= 1; ++x)
        {
            for (int y = -1; y <= 1; ++y)
            {
                if (z == 4 && x == 0 && y == 0) continue;
                blocks.push_back(TreeStampBlock(IntVec3(x, y, z), leavesId));
            }
        }
    }

    return BirchTreeStamp(blocks, "Small");
}

//-----------------------------------------------------------------------------------------------
// Create Medium Birch Tree
//
BirchTreeStamp BirchTreeStamp::CreateMedium()
{
    int logId    = BlockRegistry::GetBlockId("simpleminer", "birch_log");
    int leavesId = BlockRegistry::GetBlockId("simpleminer", "birch_leaves");

    std::vector<TreeStampBlock> blocks;

    // Trunk (6 blocks tall)
    for (int z = 0; z <= 5; ++z)
    {
        blocks.push_back(TreeStampBlock(IntVec3(0, 0, z), logId));
    }

    // Leaves (Z=5-6, 3x3)
    for (int z = 5; z <= 6; ++z)
    {
        for (int x = -1; x <= 1; ++x)
        {
            for (int y = -1; y <= 1; ++y)
            {
                if (z == 5 && x == 0 && y == 0) continue;
                blocks.push_back(TreeStampBlock(IntVec3(x, y, z), leavesId));
            }
        }
    }

    return BirchTreeStamp(blocks, "Medium");
}

//-----------------------------------------------------------------------------------------------
// Create Large Birch Tree
//
BirchTreeStamp BirchTreeStamp::CreateLarge()
{
    int logId    = BlockRegistry::GetBlockId("simpleminer", "birch_log");
    int leavesId = BlockRegistry::GetBlockId("simpleminer", "birch_leaves");

    std::vector<TreeStampBlock> blocks;

    // Trunk (7 blocks tall)
    for (int z = 0; z <= 6; ++z)
    {
        blocks.push_back(TreeStampBlock(IntVec3(0, 0, z), logId));
    }

    // Leaves (Z=6-7, 3x3)
    for (int z = 6; z <= 7; ++z)
    {
        for (int x = -1; x <= 1; ++x)
        {
            for (int y = -1; y <= 1; ++y)
            {
                if (z == 6 && x == 0 && y == 0) continue;
                blocks.push_back(TreeStampBlock(IntVec3(x, y, z), leavesId));
            }
        }
    }

    return BirchTreeStamp(blocks, "Large");
}
