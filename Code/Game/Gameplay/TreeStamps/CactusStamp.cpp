#include "Game/Gameplay/TreeStamps/CactusStamp.hpp"
#include "Engine/Registry/Block/BlockRegistry.hpp"

//-----------------------------------------------------------------------------------------------
// Constructor
//
CactusStamp::CactusStamp(const std::vector<TreeStampBlock>& blocks, const std::string& sizeName)
    : TreeStamp(blocks)
      , m_sizeName(sizeName)
{
    // Initialize Block IDs from BlockRegistry
    // Cactus only uses one block type
    InitializeBlockIds(GetLogBlockName(), "");
}

//-----------------------------------------------------------------------------------------------
// Create Small Cactus
//
// Coordinate System:
// - SimpleMiner: X=Forward, Y=Left, Z=Up
// - Origin at ground level (Z=0)
//
using namespace enigma::registry::block;

CactusStamp CactusStamp::CreateSmall()
{
    // Small Cactus: 2 blocks tall, simple vertical column
    int cactusId = BlockRegistry::GetBlockId("simpleminer", "cactus");

    std::vector<TreeStampBlock> blocks;

    // Main trunk (2 blocks)
    blocks.push_back(TreeStampBlock(IntVec3(0, 0, 0), cactusId));
    blocks.push_back(TreeStampBlock(IntVec3(0, 0, 1), cactusId));

    return CactusStamp(blocks, "Small");
}

//-----------------------------------------------------------------------------------------------
// Create Medium Cactus
//
CactusStamp CactusStamp::CreateMedium()
{
    // Medium Cactus: 3 blocks tall with 1 side branch
    int cactusId = BlockRegistry::GetBlockId("simpleminer", "cactus");

    std::vector<TreeStampBlock> blocks;

    // Main trunk (3 blocks)
    blocks.push_back(TreeStampBlock(IntVec3(0, 0, 0), cactusId));
    blocks.push_back(TreeStampBlock(IntVec3(0, 0, 1), cactusId));
    blocks.push_back(TreeStampBlock(IntVec3(0, 0, 2), cactusId));

    // Add 1 side branch at height 2, extending in +X direction
    blocks.push_back(TreeStampBlock(IntVec3(1, 0, 2), cactusId));

    return CactusStamp(blocks, "Medium");
}

//-----------------------------------------------------------------------------------------------
// Create Large Cactus
//
CactusStamp CactusStamp::CreateLarge()
{
    // Large Cactus: 4 blocks tall with 2 side branches forming classic shape
    int cactusId = BlockRegistry::GetBlockId("simpleminer", "cactus");

    std::vector<TreeStampBlock> blocks;

    // Main trunk (4 blocks)
    blocks.push_back(TreeStampBlock(IntVec3(0, 0, 0), cactusId));
    blocks.push_back(TreeStampBlock(IntVec3(0, 0, 1), cactusId));
    blocks.push_back(TreeStampBlock(IntVec3(0, 0, 2), cactusId));
    blocks.push_back(TreeStampBlock(IntVec3(0, 0, 3), cactusId));

    // Left branch at height 2, extending in +Y direction
    blocks.push_back(TreeStampBlock(IntVec3(0, 1, 2), cactusId));
    blocks.push_back(TreeStampBlock(IntVec3(0, 1, 3), cactusId)); // Extend upward

    // Right branch at height 3, extending in -Y direction
    blocks.push_back(TreeStampBlock(IntVec3(0, -1, 3), cactusId));

    return CactusStamp(blocks, "Large");
}
