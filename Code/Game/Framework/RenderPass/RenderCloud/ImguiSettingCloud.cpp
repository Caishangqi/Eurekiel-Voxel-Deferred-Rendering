/**
 * @file ImguiSettingCloud.cpp
 * @brief [FIX] Static ImGui interface for CloudRenderPass debugging - Implementation
 * @date 2025-12-02
 *
 * [FIX] Updated to match new CloudRenderPass architecture:
 * - Use GetRenderMode()/SetRenderMode() instead of Get/SetFancyMode()
 * - CloudStatus enum: FAST/FANCY modes
 * - Removed non-existent GetCloudSpeed/Opacity methods
 * - Added debug info display (ViewOrientation, geometry stats)
 *
 * Implementation Notes:
 * - Uses ImGui::CollapsingHeader for collapsible section
 * - ImGui::RadioButton for FAST/FANCY mode selection
 * - ImGui::Text for read-only debug info display
 *
 * Reference:
 * - CloudRenderPass.hpp (CloudStatus, ViewOrientation enums)
 * - ImguiSettingSky.hpp (Similar pattern)
 * - imgui_demo.cpp (Official API examples)
 */

#include "ImguiSettingCloud.hpp"
#include "CloudRenderPass.hpp"
#include "ThirdParty/imgui/imgui.h"

/**
 * @brief Show - Render cloud rendering debug UI
 *
 * [FIX] Updated UI Flow:
 * 1. Check cloudPass validity
 * 2. Render CollapsingHeader "Cloud Rendering"
 * 3. Display FAST/FANCY mode radio buttons
 * 4. Display debug info (ViewOrientation, geometry stats)
 * 5. Display cloud layer info (height, render distance)
 */
void ImguiSettingCloud::Show(CloudRenderPass* cloudPass)
{
    // [STEP 1] Validate CloudRenderPass pointer
    if (!cloudPass)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "[ERROR] CloudRenderPass is null");
        return;
    }

    // [STEP 2] CollapsingHeader: Collapsible section for cloud rendering parameters
    // Returns true if expanded, false if collapsed
    if (ImGui::CollapsingHeader("Cloud Rendering"))
    {
        // Indent content for better visual hierarchy
        ImGui::Indent();

        // ==================== [FIX] Rendering Mode Selection ====================

        ImGui::Text("Rendering Mode:");
        ImGui::Spacing();

        // [FIX] Get current mode from CloudRenderPass
        CloudStatus currentMode = cloudPass->GetRenderMode();
        bool        isFast      = (currentMode == CloudStatus::FAST);
        bool        isFancy     = (currentMode == CloudStatus::FANCY);

        // [FIX] Use RadioButton for mutually exclusive selection
        if (ImGui::RadioButton("FAST Mode", isFast))
        {
            cloudPass->SetRenderMode(CloudStatus::FAST);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Single flat face per cell (~32K vertices)");
        }

        ImGui::SameLine();

        if (ImGui::RadioButton("FANCY Mode", isFancy))
        {
            cloudPass->SetRenderMode(CloudStatus::FANCY);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Full volumetric cells (~98K vertices)");
        }

        ImGui::Separator();

        // ==================== [NEW] Debug Info Display ====================

        if (ImGui::CollapsingHeader("Debug Info"))
        {
            ImGui::Indent();

            // [NEW] Display current rendering mode status
            ImGui::Text("Current Mode:");
            ImGui::SameLine();
            if (currentMode == CloudStatus::FAST)
            {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "FAST (Flat)");
            }
            else
            {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "FANCY (Volumetric)");
            }

            ImGui::Spacing();

            // [NEW] ViewOrientation display (placeholder - needs CloudRenderPass API)
            ImGui::Text("View Orientation: [TODO: Add GetViewOrientation() API]");
            ImGui::BulletText("BELOW_CLOUDS: Camera below Z=192");
            ImGui::BulletText("INSIDE_CLOUDS: Camera at Z=192-196");
            ImGui::BulletText("ABOVE_CLOUDS: Camera above Z=196");

            ImGui::Spacing();

            // [NEW] Geometry statistics display (placeholder - needs CloudRenderPass API)
            ImGui::Text("Geometry Statistics:");
            ImGui::BulletText("Vertex Count: [TODO: Add GetVertexCount() API]");
            ImGui::BulletText("Cell Count: 32x32 grid");
            ImGui::BulletText("Rebuild Count: [TODO: Add GetRebuildCount() API]");

            ImGui::Unindent();
        }

        ImGui::Separator();

        // ==================== Read-Only Cloud Layer Info ====================

        ImGui::Text("Cloud Layer Info:");
        ImGui::Spacing();

        // Cloud height range
        ImGui::BulletText("Height: Z=192-196 (4 blocks thick)");

        // Render distance info (placeholder - needs game settings API)
        ImGui::BulletText("Render Distance: [TODO: Read from game settings]");

        // Coordinate system reminder
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "Coordinate System Mapping:\n"
                "Minecraft +X (East) -> Engine +Y (Left)\n"
                "Minecraft +Y (Up) -> Engine +Z (Up)\n"
                "Minecraft +Z (South) -> Engine +X (Forward)"
            );
        }

        ImGui::Unindent();
    }
}
