#pragma once
#include "Engine/Voxel/Feature/TreeStamp.hpp"

//-----------------------------------------------------------------------------------------------
// JungleTreeStamp: Jungle tree template (bushy)
//
class JungleTreeStamp : public TreeStamp
{
public:
    JungleTreeStamp() = default;
    explicit JungleTreeStamp(const std::vector<TreeStampBlock>& blocks, const std::string& sizeName = "Medium");

    // TreeStamp interface
    virtual std::string GetTypeName() const override { return "Jungle"; }
    virtual std::string GetSizeName() const override { return m_sizeName; }

    // Block name interface
    virtual std::string GetLogBlockName() const override { return "jungle_log"; }
    virtual std::string GetLeavesBlockName() const override { return "jungle_leaves"; }

    // Static factory methods
    static JungleTreeStamp CreateSmall();
    static JungleTreeStamp CreateMedium();
    static JungleTreeStamp CreateLarge();

private:
    std::string m_sizeName = "Medium";
};
