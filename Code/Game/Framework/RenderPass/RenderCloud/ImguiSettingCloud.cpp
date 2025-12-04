/**
 * @file ImguiSettingCloud.cpp
 * @brief Static ImGui interface for CloudRenderPass configuration
 * @date 2025-12-04
 */

#include "ImguiSettingCloud.hpp"
#include "CloudConfigParser.hpp"
#include "CloudRenderPass.hpp"
#include "ThirdParty/imgui/imgui.h"

void ImguiSettingCloud::Show(CloudRenderPass* cloudPass)
{
    if (!cloudPass)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "[ERROR] CloudRenderPass is null");
        return;
    }

    // Get config reference from CloudRenderPass
    CloudConfig& config = cloudPass->GetConfig();

    if (ImGui::CollapsingHeader("Cloud Rendering"))
    {
        ImGui::Indent();

        // ==================== Enable/Disable ====================
        ImGui::Checkbox("Enable Clouds", &config.enabled);
        ImGui::Separator();

        // ==================== Rendering Mode ====================
        ImGui::Text("Rendering Mode:");
        ImGui::Spacing();

        CloudStatus currentMode = cloudPass->GetRenderMode();
        bool        isFast      = (currentMode == CloudStatus::FAST);
        bool        isFancy     = (currentMode == CloudStatus::FANCY);

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

        // ==================== Geometry Parameters ====================
        ImGui::Text("Geometry Parameters:");
        ImGui::Spacing();

        if (ImGui::SliderFloat("Height", &config.height, 0.0f, 256.0f, "%.1f"))
        {
            cloudPass->RequestRebuild();
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Cloud layer base height (Z-axis)");
        }

        if (ImGui::SliderFloat("Thickness", &config.thickness, 1.0f, 16.0f, "%.1f"))
        {
            cloudPass->RequestRebuild();
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Cloud layer thickness in blocks");
        }

        if (ImGui::SliderInt("Render Distance", &config.renderDistance, 4, 32))
        {
            cloudPass->RequestRebuild();
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Render distance in cells (1 cell = 12 blocks)");
        }

        ImGui::Separator();

        // ==================== Visual Parameters ====================
        ImGui::Text("Visual Parameters:");
        ImGui::Spacing();

        ImGui::SliderFloat("Speed", &config.speed, 0.0f, 5.0f, "%.2f");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Cloud scroll speed multiplier");
        }

        ImGui::SliderFloat("Opacity", &config.opacity, 0.0f, 1.0f, "%.2f");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Cloud transparency (0.0 = invisible, 1.0 = opaque)");
        }

        ImGui::Separator();

        // ==================== Debug Info ====================
        if (ImGui::CollapsingHeader("Debug Info"))
        {
            ImGui::Indent();

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
            ImGui::Text("Cloud Layer:");
            ImGui::BulletText("Min Z: %.1f", config.GetMinZ());
            ImGui::BulletText("Max Z: %.1f", config.GetMaxZ());
            ImGui::BulletText("Radius: %d cells (%d blocks)", config.renderDistance, config.renderDistance * 12);

            ImGui::Unindent();
        }

        ImGui::Unindent();
    }
}
