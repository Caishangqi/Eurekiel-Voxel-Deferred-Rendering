#pragma once

// Forward declaration
class SkyRenderPass;

/**
 * ImguiSettingSky - Static ImGui interface for SkyRenderPass debugging
 *
 * Purpose:
 * - Display sky rendering parameters (Void gradient, sky colors)
 * - Provide runtime adjustment controls for debugging
 * - CollapsingHeader format for clean UI organization
 *
 * Design Pattern:
 * - Static-only class (no instantiation)
 * - Stateless UI rendering (reads from SkyRenderPass directly)
 * - Follows ImGui immediate mode paradigm
 *
 * Usage:
 * // In ImguiSceneRendering::Show():
 * if (ImGui::CollapsingHeader("Sky Rendering"))
 * {
 *     ImguiSettingSky::Show(skyRenderPass);
 * }
 *
 * Reference:
 * - F:/p4/Personal/SD/Engine/Code/ThirdParty/imgui/imgui_demo.cpp (Official API examples)
 * - ImGui::Checkbox, ImGui::ColorEdit3, ImGui::Text, ImGui::CollapsingHeader
 * - Code/Game/Framework/Time/ImguiSettingTime.hpp (Similar pattern)
 */
class ImguiSettingSky
{
public:
    // [IMPORTANT] Static-only class - no instantiation allowed
    ImguiSettingSky()                                  = delete;
    ImguiSettingSky(const ImguiSettingSky&)            = delete;
    ImguiSettingSky& operator=(const ImguiSettingSky&) = delete;

    /**
     * Show - Render sky rendering debug UI in a CollapsingHeader
     *
     * Parameters:
     * - skyPass: SkyRenderPass instance to display/modify
     *
     * UI Layout:
     * - CollapsingHeader: "Sky Rendering"
     *   - Checkbox: Enable Void Gradient (toggle on/off)
     *   - ColorEdit3: Sky Zenith Color (RGB picker)
     *   - ColorEdit3: Sky Horizon Color (RGB picker)
     *   - Text: Current Sun Position (read-only display)
     *   - Text: Current Moon Position (read-only display)
     */
    static void Show(SkyRenderPass* skyPass);
};
