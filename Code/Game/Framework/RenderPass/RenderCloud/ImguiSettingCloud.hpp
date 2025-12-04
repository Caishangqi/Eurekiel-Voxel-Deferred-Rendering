/**
 * @file ImguiSettingCloud.hpp
 * @brief [FIX] Static ImGui interface for CloudRenderPass debugging
 * @date 2025-12-02
 *
 * [FIX] Updated to match new CloudRenderPass architecture:
 * - Use GetRenderMode()/SetRenderMode() API
 * - CloudStatus enum: FAST/FANCY modes
 * - Added debug info display (ViewOrientation, geometry stats)
 *
 * Purpose:
 * - Display cloud rendering mode (FAST/FANCY)
 * - Provide runtime mode switching controls
 * - Display debug info (camera orientation, geometry stats)
 * - CollapsingHeader format for clean UI organization
 *
 * Design Pattern:
 * - Static-only class (no instantiation)
 * - Stateless UI rendering (reads from CloudRenderPass directly)
 * - Follows ImGui immediate mode paradigm
 *
 * Usage:
 * @code
 * // In ImguiSceneRendering::Show():
 * if (ImGui::CollapsingHeader("Cloud Rendering"))
 * {
 *     ImguiSettingCloud::Show(cloudRenderPass);
 * }
 * @endcode
 *
 * Reference:
 * - CloudRenderPass.hpp (CloudStatus, ViewOrientation enums)
 * - ImguiSettingSky.hpp (Similar pattern)
 * - imgui_demo.cpp (Official API examples)
 */

#pragma once

// Forward declaration
class CloudRenderPass;

/**
 * @class ImguiSettingCloud
 * @brief [FIX] Static ImGui interface for CloudRenderPass debugging
 *
 * [FIX] Updated UI Layout:
 * - CollapsingHeader: "Cloud Rendering"
 *   - RadioButton: FAST/FANCY mode selection
 *   - CollapsingHeader: "Debug Info"
 *     - Current rendering mode status
 *     - ViewOrientation display (BELOW/INSIDE/ABOVE clouds)
 *     - Geometry statistics (vertex count, cell count, rebuild count)
 *   - Cloud layer info (height, render distance, coordinate system)
 */
class ImguiSettingCloud
{
public:
    // [IMPORTANT] Static-only class - no instantiation allowed
    ImguiSettingCloud()                                    = delete;
    ImguiSettingCloud(const ImguiSettingCloud&)            = delete;
    ImguiSettingCloud& operator=(const ImguiSettingCloud&) = delete;

    /**
     * @brief Show - Render cloud rendering debug UI in a CollapsingHeader
     * @param cloudPass CloudRenderPass instance to display/modify
     *
     * [FIX] Updated UI Controls:
     * - Rendering Mode: RadioButton for FAST (~32K vertices) / FANCY (~98K vertices)
     * - Debug Info: ViewOrientation, vertex count, rebuild count (TODO: needs API)
     * - Cloud Layer Info: Height range, render distance, coordinate system mapping
     */
    static void Show(CloudRenderPass* cloudPass);
};
