#pragma once
#include "Engine/Voxel/Feature/TreeStamp.hpp"

//-----------------------------------------------------------------------------------------------
// AcaciaTreeStamp: Acacia tree template (angled trunk)
//
class AcaciaTreeStamp : public TreeStamp
{
public:
    AcaciaTreeStamp() = default;
    explicit AcaciaTreeStamp(const std::vector<TreeStampBlock>& blocks, const std::string& sizeName = "Medium");

    // TreeStamp interface
    virtual std::string GetTypeName() const override { return "Acacia"; }
    virtual std::string GetSizeName() const override { return m_sizeName; }

    // Block name interface
    virtual std::string GetLogBlockName() const override { return "acacia_log"; }
    virtual std::string GetLeavesBlockName() const override { return "acacia_leaves"; }

    // Static factory methods
    static AcaciaTreeStamp CreateSmall();
    static AcaciaTreeStamp CreateMedium();
    static AcaciaTreeStamp CreateLarge();

private:
    std::string m_sizeName = "Medium";
};
