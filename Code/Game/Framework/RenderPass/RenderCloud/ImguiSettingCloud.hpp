/**
 * @file ImguiSettingCloud.hpp
 * @brief Static ImGui interface for CloudRenderPass debugging
 * @date 2025-11-26
 *
 * Purpose:
 * - Display cloud rendering parameters (speed, opacity, mode)
 * - Provide runtime adjustment controls for debugging
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
 * - F:/p4/Personal/SD/Engine/Code/ThirdParty/imgui/imgui_demo.cpp (Official API examples)
 * - ImGui::SliderFloat, ImGui::Checkbox, ImGui::Text, ImGui::CollapsingHeader
 * - Code/Game/Framework/RenderPass/RenderSky/ImguiSettingSky.hpp (Similar pattern)
 */

#pragma once

// Forward declaration
class CloudRenderPass;

/**
 * @class ImguiSettingCloud
 * @brief Static ImGui interface for CloudRenderPass debugging
 *
 * UI Layout:
 * - CollapsingHeader: "Cloud Rendering"
 *   - SliderFloat: Cloud Speed (0.0 - 5.0)
 *   - SliderFloat: Cloud Opacity (0.0 - 1.0)
 *   - Checkbox: Fancy Mode (Fast: 6144 verts, Fancy: 24576 verts)
 *   - Text: Current Vertex Count (read-only)
 *   - Button: Reset to Defaults
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
     * UI Controls:
     * - Cloud Speed: Controls animation speed multiplier (default: 1.0)
     * - Cloud Opacity: Controls transparency (0.0 = transparent, 1.0 = opaque, default: 0.8)
     * - Fancy Mode: Toggle between Fast (6144 verts) and Fancy (24576 verts) modes
     * - Vertex Count: Display current vertex count (read-only)
     * - Reset Button: Reset all parameters to defaults
     */
    static void Show(CloudRenderPass* cloudPass);
};
