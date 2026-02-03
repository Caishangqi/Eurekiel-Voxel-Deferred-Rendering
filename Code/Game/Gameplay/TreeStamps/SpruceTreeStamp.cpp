#include "Game/Gameplay/TreeStamps/SpruceTreeStamp.hpp"
#include "Engine/Registry/Block/BlockRegistry.hpp"
using namespace enigma::registry::block;

SpruceTreeStamp::SpruceTreeStamp(const std::vector<TreeStampBlock>& blocks, const std::string& sizeName)
    : TreeStamp(blocks), m_sizeName(sizeName)
{
    // Initialize Block IDs from BlockRegistry
    InitializeBlockIds(GetLogBlockName(), GetLeavesBlockName());
}

SpruceTreeStamp SpruceTreeStamp::CreateSmall()
{
    int logId    = BlockRegistry::GetBlockId("simpleminer", "spruce_log");
    int leavesId = BlockRegistry::GetBlockId("simpleminer", "spruce_leaves");

    std::vector<TreeStampBlock> blocks;

    // Trunk (6 blocks tall)
    for (int z = 0; z <= 5; ++z)
        blocks.push_back(TreeStampBlock(IntVec3(0, 0, z), logId));

    // Leaves layers (conical shape)
    for (int z = 2; z <= 4; ++z)
    {
        for (int x = -1; x <= 1; ++x)
        {
            for (int y = -1; y <= 1; ++y)
            {
                if (x == 0 && y == 0) continue;
                blocks.push_back(TreeStampBlock(IntVec3(x, y, z), leavesId));
            }
        }
    }
    blocks.push_back(TreeStampBlock(IntVec3(0, 0, 6), leavesId));
    return SpruceTreeStamp(blocks, "Small");
}

SpruceTreeStamp SpruceTreeStamp::CreateMedium()
{
    int logId    = BlockRegistry::GetBlockId("simpleminer", "spruce_log");
    int leavesId = BlockRegistry::GetBlockId("simpleminer", "spruce_leaves");

    std::vector<TreeStampBlock> blocks;

    // Trunk (8 blocks tall)
    for (int z = 0; z <= 7; ++z)
        blocks.push_back(TreeStampBlock(IntVec3(0, 0, z), logId));

    // Leaves layers (diamond pattern)
    for (int z = 3; z <= 6; ++z)
    {
        int radius = (z <= 4) ? 2 : 1;
        for (int x = -radius; x <= radius; ++x)
        {
            for (int y = -radius; y <= radius; ++y)
            {
                int dist = abs(x) + abs(y);
                if (dist <= radius && !(x == 0 && y == 0))
                    blocks.push_back(TreeStampBlock(IntVec3(x, y, z), leavesId));
            }
        }
    }
    blocks.push_back(TreeStampBlock(IntVec3(0, 0, 8), leavesId));
    return SpruceTreeStamp(blocks, "Medium");
}

SpruceTreeStamp SpruceTreeStamp::CreateLarge()
{
    int logId    = BlockRegistry::GetBlockId("simpleminer", "spruce_log");
    int leavesId = BlockRegistry::GetBlockId("simpleminer", "spruce_leaves");

    std::vector<TreeStampBlock> blocks;

    // Trunk (10 blocks tall)
    for (int z = 0; z <= 9; ++z)
        blocks.push_back(TreeStampBlock(IntVec3(0, 0, z), logId));

    // Leaves layers (large conical shape)
    for (int z = 4; z <= 8; ++z)
    {
        int radius = (z <= 6) ? 2 : 1;
        for (int x = -radius; x <= radius; ++x)
        {
            for (int y = -radius; y <= radius; ++y)
            {
                int dist = abs(x) + abs(y);
                if (dist <= radius && !(x == 0 && y == 0))
                    blocks.push_back(TreeStampBlock(IntVec3(x, y, z), leavesId));
            }
        }
    }
    blocks.push_back(TreeStampBlock(IntVec3(0, 0, 10), leavesId));
    return SpruceTreeStamp(blocks, "Large");
}
