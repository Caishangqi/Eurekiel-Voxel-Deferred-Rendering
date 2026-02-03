#pragma once
#include "Engine/Voxel/Feature/TreeStamp.hpp"

//-----------------------------------------------------------------------------------------------
// OakTreeStamp: Oak tree template
//
class OakTreeStamp : public TreeStamp
{
public:
    OakTreeStamp() = default;
    explicit OakTreeStamp(const std::vector<TreeStampBlock>& blocks, const std::string& sizeName = "Medium");

    // TreeStamp interface
    virtual std::string GetTypeName() const override { return "Oak"; }
    virtual std::string GetSizeName() const override { return m_sizeName; }

    // Block name interface
    virtual std::string GetLogBlockName() const override { return "oak_log"; }
    virtual std::string GetLeavesBlockName() const override { return "oak_leaves"; }

    // Static factory methods
    static OakTreeStamp CreateSmall();
    static OakTreeStamp CreateMedium();
    static OakTreeStamp CreateLarge();

private:
    std::string m_sizeName = "Medium";
};
