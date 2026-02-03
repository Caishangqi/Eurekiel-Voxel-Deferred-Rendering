#pragma once
#include "Engine/Voxel/Feature/TreeStamp.hpp"

//-----------------------------------------------------------------------------------------------
// SpruceTreeStamp: Spruce tree template (conical shape)
//
class SpruceTreeStamp : public TreeStamp
{
public:
    SpruceTreeStamp() = default;
    explicit SpruceTreeStamp(const std::vector<TreeStampBlock>& blocks, const std::string& sizeName = "Medium");

    // TreeStamp interface
    virtual std::string GetTypeName() const override { return "Spruce"; }
    virtual std::string GetSizeName() const override { return m_sizeName; }

    // Block name interface
    virtual std::string GetLogBlockName() const override { return "spruce_log"; }
    virtual std::string GetLeavesBlockName() const override { return "spruce_leaves"; }

    // Static factory methods
    static SpruceTreeStamp CreateSmall();
    static SpruceTreeStamp CreateMedium();
    static SpruceTreeStamp CreateLarge();

private:
    std::string m_sizeName = "Medium";
};
