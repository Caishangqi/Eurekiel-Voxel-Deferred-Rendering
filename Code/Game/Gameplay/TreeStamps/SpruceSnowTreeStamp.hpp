#pragma once
#include "Engine/Voxel/Feature/TreeStamp.hpp"

//-----------------------------------------------------------------------------------------------
// SpruceSnowTreeStamp: Snowy spruce tree template for snowy taiga biomes
// Uses spruce_leaves_snow blocks instead of regular spruce_leaves
//
class SpruceSnowTreeStamp : public TreeStamp
{
public:
    SpruceSnowTreeStamp() = default;
    explicit SpruceSnowTreeStamp(const std::vector<TreeStampBlock>& blocks, const std::string& sizeName = "Medium");

    // TreeStamp interface
    virtual std::string GetTypeName() const override { return "SpruceSnow"; }
    virtual std::string GetSizeName() const override { return m_sizeName; }

    // Block name interface
    virtual std::string GetLogBlockName() const override { return "spruce_log"; }
    virtual std::string GetLeavesBlockName() const override { return "spruce_leaves_snow"; }

    // Static factory methods
    static SpruceSnowTreeStamp CreateSmall();
    static SpruceSnowTreeStamp CreateMedium();
    static SpruceSnowTreeStamp CreateLarge();

private:
    std::string m_sizeName = "Medium";
};
