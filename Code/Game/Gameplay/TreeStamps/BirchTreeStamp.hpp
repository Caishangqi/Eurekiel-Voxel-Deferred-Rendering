#pragma once
#include "Engine/Voxel/Feature/TreeStamp.hpp"

//-----------------------------------------------------------------------------------------------
// BirchTreeStamp: Birch tree template
//
class BirchTreeStamp : public TreeStamp
{
public:
    BirchTreeStamp() = default;
    explicit BirchTreeStamp(const std::vector<TreeStampBlock>& blocks, const std::string& sizeName = "Medium");

    // TreeStamp interface
    virtual std::string GetTypeName() const override { return "Birch"; }
    virtual std::string GetSizeName() const override { return m_sizeName; }

    // Block name interface
    virtual std::string GetLogBlockName() const override { return "birch_log"; }
    virtual std::string GetLeavesBlockName() const override { return "birch_leaves"; }

    // Static factory methods
    static BirchTreeStamp CreateSmall();
    static BirchTreeStamp CreateMedium();
    static BirchTreeStamp CreateLarge();

private:
    std::string m_sizeName = "Medium";
};
