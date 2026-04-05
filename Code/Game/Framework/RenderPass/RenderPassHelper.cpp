#include "RenderPassHelper.hpp"

std::vector<std::pair<RenderTargetType, int>> RenderPassHelper::GetRenderTargetColorFromIndex(const std::vector<uint32_t>& inIntIndexList, RenderTargetType inRTType)
{
    std::vector<std::pair<RenderTargetType, int>> result;
    if (inIntIndexList.size() == 0)
        return result;
    result.reserve(inIntIndexList.size());
    for (int value : inIntIndexList)
    {
        result.push_back({inRTType, value});
    }
    return result;
}

std::vector<std::pair<RenderTargetType, int>> RenderPassHelper::BuildRenderTargets(
    const std::vector<uint32_t>& inColorIndexList,
    RenderTargetType             inColorRTType,
    RenderTargetType             inDepthRTType,
    int                          inDepthIndex)
{
    std::vector<std::pair<RenderTargetType, int>> result = GetRenderTargetColorFromIndex(inColorIndexList, inColorRTType);
    result.push_back({inDepthRTType, inDepthIndex});
    return result;
}
