/**
 * @file ImguiSettingCloud.hpp
 * @brief Static ImGui interface for CloudRenderPass configuration
 * @date 2025-12-04
 *
 * Design Pattern:
 * - Static-only class (no instantiation)
 * - Accesses CloudConfig via CloudRenderPass::GetConfig()
 * - Follows ImGui immediate mode paradigm
 */

#pragma once

// Forward declarations
class CloudRenderPass;

/**
 * @class ImguiSettingCloud
 * @brief Static ImGui interface for cloud rendering configuration
 */
class ImguiSettingCloud
{
public:
    ImguiSettingCloud()                                    = delete;
    ImguiSettingCloud(const ImguiSettingCloud&)            = delete;
    ImguiSettingCloud& operator=(const ImguiSettingCloud&) = delete;

    /**
     * @brief Show cloud rendering configuration UI
     * @param cloudPass CloudRenderPass instance (provides config access)
     */
    static void Show(CloudRenderPass* cloudPass);
};
