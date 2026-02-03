#include "Game/Gameplay/TreeStamps/JungleTreeStamp.hpp"
#include "Engine/Registry/Block/BlockRegistry.hpp"
using namespace enigma::registry::block;

JungleTreeStamp::JungleTreeStamp(const std::vector<TreeStampBlock>& blocks, const std::string& sizeName)
    : TreeStamp(blocks), m_sizeName(sizeName)
{
    // Initialize Block IDs from BlockRegistry
    InitializeBlockIds(GetLogBlockName(), GetLeavesBlockName());
}

JungleTreeStamp JungleTreeStamp::CreateSmall()
{
    // Query Block IDs directly from BlockRegistry
    int logId    = BlockRegistry::GetBlockId("simpleminer", "jungle_log");
    int leavesId = BlockRegistry::GetBlockId("simpleminer", "jungle_leaves");

    std::vector<TreeStampBlock> blocks;
    // Trunk (5 blocks tall)
    for (int z = 0; z <= 4; ++z)
        blocks.push_back(TreeStampBlock(IntVec3(0, 0, z), logId));

    // Leaves (Z=3-5, 3x3 bushy)
    for (int z = 3; z <= 5; ++z)
    {
        for (int x = -1; x <= 1; ++x)
        {
            for (int y = -1; y <= 1; ++y)
            {
                if (z < 5 && x == 0 && y == 0) continue;
                blocks.push_back(TreeStampBlock(IntVec3(x, y, z), leavesId));
            }
        }
    }
    return JungleTreeStamp(blocks, "Small");
}

JungleTreeStamp JungleTreeStamp::CreateMedium()
{
    // Query Block IDs directly from BlockRegistry
    int logId    = BlockRegistry::GetBlockId("simpleminer", "jungle_log");
    int leavesId = BlockRegistry::GetBlockId("simpleminer", "jungle_leaves");

    std::vector<TreeStampBlock> blocks;
    // Trunk (7 blocks tall)
    for (int z = 0; z <= 6; ++z)
        blocks.push_back(TreeStampBlock(IntVec3(0, 0, z), logId));

    // Leaves (Z=5-7, 5x5 bushy)
    for (int z = 5; z <= 7; ++z)
    {
        int radius = (z == 7) ? 1 : 2;
        for (int x = -radius; x <= radius; ++x)
        {
            for (int y = -radius; y <= radius; ++y)
            {
                if (z < 7 && x == 0 && y == 0) continue;
                blocks.push_back(TreeStampBlock(IntVec3(x, y, z), leavesId));
            }
        }
    }
    return JungleTreeStamp(blocks, "Medium");
}

JungleTreeStamp JungleTreeStamp::CreateLarge()
{
    // Query Block IDs directly from BlockRegistry
    int logId    = BlockRegistry::GetBlockId("simpleminer", "jungle_log");
    int leavesId = BlockRegistry::GetBlockId("simpleminer", "jungle_leaves");

    std::vector<TreeStampBlock> blocks;
    // Trunk (9 blocks tall)
    for (int z = 0; z <= 8; ++z)
        blocks.push_back(TreeStampBlock(IntVec3(0, 0, z), logId));

    // Leaves (Z=6-9, very bushy)
    for (int z = 6; z <= 9; ++z)
    {
        int radius = (z == 9) ? 1 : 2;
        for (int x = -radius; x <= radius; ++x)
        {
            for (int y = -radius; y <= radius; ++y)
            {
                if (z < 9 && x == 0 && y == 0) continue;
                blocks.push_back(TreeStampBlock(IntVec3(x, y, z), leavesId));
            }
        }
    }
    return JungleTreeStamp(blocks, "Large");
}
