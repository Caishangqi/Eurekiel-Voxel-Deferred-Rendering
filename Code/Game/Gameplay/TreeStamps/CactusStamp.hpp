#pragma once
#include "Engine/Voxel/Feature/TreeStamp.hpp"

//-----------------------------------------------------------------------------------------------
// CactusStamp: Cactus template for desert biomes
//
class CactusStamp : public TreeStamp
{
public:
    CactusStamp() = default;
    explicit CactusStamp(const std::vector<TreeStampBlock>& blocks, const std::string& sizeName = "Medium");

    // TreeStamp interface
    virtual std::string GetTypeName() const override { return "Cactus"; }
    virtual std::string GetSizeName() const override { return m_sizeName; }

    // Block name interface
    // Cactus only uses cactus block (no log/leaves distinction)
    virtual std::string GetLogBlockName() const override { return "cactus"; }
    virtual std::string GetLeavesBlockName() const override { return ""; }

    // Static factory methods
    static CactusStamp CreateSmall();
    static CactusStamp CreateMedium();
    static CactusStamp CreateLarge();

private:
    std::string m_sizeName = "Medium";
};
