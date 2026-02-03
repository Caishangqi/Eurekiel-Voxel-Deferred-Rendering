#pragma once
#include <utility>
#include <vector>

namespace enigma::graphic
{
    enum class RenderTargetType;
}

using namespace enigma::graphic;

class RenderPassHelper
{
public:
    RenderPassHelper()                                   = delete; // Prevent instantiation
    RenderPassHelper(const RenderPassHelper&)            = delete; // Prevent copy
    RenderPassHelper& operator=(const RenderPassHelper&) = delete; // Prevent assignment
public:
    static std::vector<std::pair<RenderTargetType, int>> GetRenderTargetColorFromIndex(std::vector<uint32_t> inIntIndexList, RenderTargetType inRTType);
};
