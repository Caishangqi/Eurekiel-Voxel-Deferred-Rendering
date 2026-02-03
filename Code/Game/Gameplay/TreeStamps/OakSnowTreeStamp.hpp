#pragma once
#include "Engine/Voxel/Feature/TreeStamp.hpp"

//-----------------------------------------------------------------------------------------------
// OakSnowTreeStamp: Snowy oak tree template for snowy plains biomes
// Uses oak_leaves_snow blocks instead of regular oak_leaves
//
class OakSnowTreeStamp : public TreeStamp
{
public:
    OakSnowTreeStamp() = default;
    explicit OakSnowTreeStamp(const std::vector<TreeStampBlock>& blocks, const std::string& sizeName = "Medium");

    // TreeStamp interface
    virtual std::string GetTypeName() const override { return "OakSnow"; }
    virtual std::string GetSizeName() const override { return m_sizeName; }

    // Block name interface
    virtual std::string GetLogBlockName() const override { return "oak_log"; }
    virtual std::string GetLeavesBlockName() const override { return "oak_leaves_snow"; }

    // Static factory methods
    static OakSnowTreeStamp CreateSmall();
    static OakSnowTreeStamp CreateMedium();
    static OakSnowTreeStamp CreateLarge();

private:
    std::string m_sizeName = "Medium";
};
