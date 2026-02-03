#include "Game/Gameplay/TreeStamps/AcaciaTreeStamp.hpp"
#include "Engine/Registry/Block/BlockRegistry.hpp"
using namespace enigma::registry::block;

AcaciaTreeStamp::AcaciaTreeStamp(const std::vector<TreeStampBlock>& blocks, const std::string& sizeName)
    : TreeStamp(blocks), m_sizeName(sizeName)
{
    // Initialize Block IDs from BlockRegistry
    InitializeBlockIds(GetLogBlockName(), GetLeavesBlockName());
}

AcaciaTreeStamp AcaciaTreeStamp::CreateSmall()
{
    // Query Block IDs directly from BlockRegistry
    int logId    = BlockRegistry::GetBlockId("simpleminer", "acacia_log");
    int leavesId = BlockRegistry::GetBlockId("simpleminer", "acacia_leaves");

    std::vector<TreeStampBlock> blocks;
    // Trunk (angled)
    blocks.push_back(TreeStampBlock(IntVec3(0, 0, 0), logId));
    blocks.push_back(TreeStampBlock(IntVec3(0, 0, 1), logId));
    blocks.push_back(TreeStampBlock(IntVec3(0, 0, 2), logId));
    blocks.push_back(TreeStampBlock(IntVec3(1, 0, 3), logId));
    blocks.push_back(TreeStampBlock(IntVec3(1, 0, 4), logId));

    // Leaves (Z=4-5, centered on trunk end)
    for (int z = 4; z <= 5; ++z)
    {
        for (int x = 0; x <= 2; ++x)
        {
            for (int y = -1; y <= 1; ++y)
            {
                if (z == 4 && x == 1 && y == 0) continue;
                blocks.push_back(TreeStampBlock(IntVec3(x, y, z), leavesId));
            }
        }
    }
    return AcaciaTreeStamp(blocks, "Small");
}

AcaciaTreeStamp AcaciaTreeStamp::CreateMedium()
{
    // Query Block IDs directly from BlockRegistry
    int logId    = BlockRegistry::GetBlockId("simpleminer", "acacia_log");
    int leavesId = BlockRegistry::GetBlockId("simpleminer", "acacia_leaves");

    std::vector<TreeStampBlock> blocks;
    // Trunk (angled)
    blocks.push_back(TreeStampBlock(IntVec3(0, 0, 0), logId));
    blocks.push_back(TreeStampBlock(IntVec3(0, 0, 1), logId));
    blocks.push_back(TreeStampBlock(IntVec3(0, 0, 2), logId));
    blocks.push_back(TreeStampBlock(IntVec3(1, 0, 3), logId));
    blocks.push_back(TreeStampBlock(IntVec3(1, 0, 4), logId));
    blocks.push_back(TreeStampBlock(IntVec3(1, 0, 5), logId));

    // Leaves (Z=5-6, centered on trunk end)
    for (int z = 5; z <= 6; ++z)
    {
        for (int x = 0; x <= 2; ++x)
        {
            for (int y = -1; y <= 1; ++y)
            {
                if (z == 5 && x == 1 && y == 0) continue;
                blocks.push_back(TreeStampBlock(IntVec3(x, y, z), leavesId));
            }
        }
    }
    return AcaciaTreeStamp(blocks, "Medium");
}

AcaciaTreeStamp AcaciaTreeStamp::CreateLarge()
{
    // Query Block IDs directly from BlockRegistry
    int logId    = BlockRegistry::GetBlockId("simpleminer", "acacia_log");
    int leavesId = BlockRegistry::GetBlockId("simpleminer", "acacia_leaves");

    std::vector<TreeStampBlock> blocks;

    // Vertical trunk: 11 blocks tall (along Z axis - up)
    const int trunkHeight = 11;
    for (int z = 0; z < trunkHeight; ++z)
    {
        blocks.push_back(TreeStampBlock(IntVec3(0, 0, z), logId));
    }

    // Crown base height (2 blocks below trunk top)
    const int crownBaseZ = trunkHeight - 2;

    // Layer 1: Largest canopy layer - flat circular crown, radius 5
    const int layer1Z = crownBaseZ;
    for (int x = -5; x <= 5; ++x)
    {
        for (int y = -5; y <= 5; ++y)
        {
            float distSq = static_cast<float>(x * x + y * y);
            // Circular crown with radius 5, hollow center (for trunk)
            if (distSq <= 25.0f && distSq >= 1.0f)
            {
                blocks.push_back(TreeStampBlock(IntVec3(x, y, layer1Z), leavesId));
            }
        }
    }

    // Layer 2: Medium canopy layer - radius 4
    const int layer2Z = crownBaseZ + 1;
    for (int x = -4; x <= 4; ++x)
    {
        for (int y = -4; y <= 4; ++y)
        {
            float distSq = static_cast<float>(x * x + y * y);
            if (distSq <= 16.0f && distSq >= 1.0f)
            {
                blocks.push_back(TreeStampBlock(IntVec3(x, y, layer2Z), leavesId));
            }
        }
    }

    // Layer 3: Top canopy layer - radius 2-3
    const int layer3Z = crownBaseZ + 2;
    for (int x = -2; x <= 2; ++x)
    {
        for (int y = -2; y <= 2; ++y)
        {
            int manhattan = abs(x) + abs(y);
            if (manhattan <= 3 && manhattan >= 1)
            {
                blocks.push_back(TreeStampBlock(IntVec3(x, y, layer3Z), leavesId));
            }
        }
    }

    // Horizontal branches (4 cardinal directions: +X, -X, +Y, -Y)
    const int branchZ      = trunkHeight - 4; // Branches start 4 blocks below trunk top
    const int branchLength = 3;

    // +X direction branch
    for (int i = 1; i <= branchLength; ++i)
    {
        blocks.push_back(TreeStampBlock(IntVec3(i, 0, branchZ), logId));
    }
    // Leaf cluster at branch end
    for (int dx = -1; dx <= 1; ++dx)
    {
        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dz = -1; dz <= 1; ++dz)
            {
                if (dx == 0 && dy == 0 && dz == 0) continue; // Skip center (log position)
                blocks.push_back(TreeStampBlock(IntVec3(branchLength + dx, dy, branchZ + dz), leavesId));
            }
        }
    }

    // -X direction branch
    for (int i = 1; i <= branchLength; ++i)
    {
        blocks.push_back(TreeStampBlock(IntVec3(-i, 0, branchZ), logId));
    }
    // Leaf cluster at branch end
    for (int dx = -1; dx <= 1; ++dx)
    {
        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dz = -1; dz <= 1; ++dz)
            {
                if (dx == 0 && dy == 0 && dz == 0) continue;
                blocks.push_back(TreeStampBlock(IntVec3(-branchLength + dx, dy, branchZ + dz), leavesId));
            }
        }
    }

    // +Y direction branch
    for (int i = 1; i <= branchLength; ++i)
    {
        blocks.push_back(TreeStampBlock(IntVec3(0, i, branchZ), logId));
    }
    // Leaf cluster at branch end
    for (int dx = -1; dx <= 1; ++dx)
    {
        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dz = -1; dz <= 1; ++dz)
            {
                if (dx == 0 && dy == 0 && dz == 0) continue;
                blocks.push_back(TreeStampBlock(IntVec3(dx, branchLength + dy, branchZ + dz), leavesId));
            }
        }
    }

    // -Y direction branch
    for (int i = 1; i <= branchLength; ++i)
    {
        blocks.push_back(TreeStampBlock(IntVec3(0, -i, branchZ), logId));
    }
    // Leaf cluster at branch end
    for (int dx = -1; dx <= 1; ++dx)
    {
        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dz = -1; dz <= 1; ++dz)
            {
                if (dx == 0 && dy == 0 && dz == 0) continue;
                blocks.push_back(TreeStampBlock(IntVec3(dx, -branchLength + dy, branchZ + dz), leavesId));
            }
        }
    }

    return AcaciaTreeStamp(blocks, "Large");
}
