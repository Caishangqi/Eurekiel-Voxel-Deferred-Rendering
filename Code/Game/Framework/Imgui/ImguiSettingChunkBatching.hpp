/**
 * @file ImguiSettingChunkBatching.hpp
 * @brief Static ImGui interface for chunk batching runtime controls and stats
 */

#pragma once

namespace enigma::voxel
{
    class World;
}

class ImguiSettingChunkBatching
{
public:
    ImguiSettingChunkBatching()                                               = delete;
    ImguiSettingChunkBatching(const ImguiSettingChunkBatching&)               = delete;
    ImguiSettingChunkBatching& operator=(const ImguiSettingChunkBatching&)    = delete;

    static void Show(enigma::voxel::World* world);
};
