#include "RenderPassHelper.hpp"

std::vector<std::pair<RTType, int>> RenderPassHelper::GetRenderTargetColorFromIndex(std::vector<uint32_t> inIntIndexList, RTType inRTType)
{
    std::vector<std::pair<RTType, int>> result;
    if (inIntIndexList.size() == 0)
        return result;
    result.reserve(inIntIndexList.size());
    for (int value : inIntIndexList)
    {
        result.push_back({inRTType, value});
    }
    return result;
}
