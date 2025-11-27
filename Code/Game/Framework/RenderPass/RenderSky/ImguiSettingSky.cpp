#include "ImguiSettingSky.hpp"
#include "SkyRenderPass.hpp"
#include "Engine/Math/Vec3.hpp"
#include "ThirdParty/imgui/imgui.h"

/**
 * Show - Render sky rendering debug UI
 *
 * Implementation Notes:
 * - Uses ImGui::CollapsingHeader for collapsible section
 * - ImGui::Checkbox for boolean toggle (Void gradient)
 * - ImGui::ColorEdit3 for RGB color picker (sky colors)
 * - ImGui::Text for read-only parameter display (sun/moon positions)
 *
 * Reference:
 * - imgui_demo.cpp:434 (CollapsingHeader example)
 * - imgui_demo.cpp:1089 (Checkbox example)
 * - imgui_demo.cpp:1234 (ColorEdit3 example)
 */
void ImguiSettingSky::Show(SkyRenderPass* skyPass)
{
    if (!skyPass)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "[ERROR] SkyRenderPass is null");
        return;
    }

    // CollapsingHeader: Collapsible section for sky rendering parameters
    // Returns true if expanded, false if collapsed
    if (ImGui::CollapsingHeader("Sky Rendering"))
    {
        // Indent content for better visual hierarchy
        ImGui::Indent();

        // ==================== Void Gradient Toggle ====================

        bool enableVoidGradient = skyPass->IsVoidGradientEnabled();
        if (ImGui::Checkbox("Enable Void Gradient", &enableVoidGradient))
        {
            skyPass->SetVoidGradientEnabled(enableVoidGradient);
        }

        // Help text for Void gradient
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Darkens sky when camera is below Y=-64 (Minecraft style)");
        }

        ImGui::Separator();

        // ==================== Sky Color Adjustment ====================

        ImGui::Text("Sky Colors:");
        ImGui::Spacing();

        // Sky Zenith Color (top of sky)
        Vec3  zenithColor         = skyPass->GetSkyZenithColor();
        float zenithColorArray[3] = {zenithColor.x, zenithColor.y, zenithColor.z};

        if (ImGui::ColorEdit3("Zenith Color", zenithColorArray))
        {
            skyPass->SetSkyZenithColor(Vec3(zenithColorArray[0], zenithColorArray[1], zenithColorArray[2]));
        }

        // Help text for zenith color
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Color at the top of the sky (default: deep blue)");
        }

        // Sky Horizon Color (bottom of sky)
        Vec3  horizonColor         = skyPass->GetSkyHorizonColor();
        float horizonColorArray[3] = {horizonColor.x, horizonColor.y, horizonColor.z};

        if (ImGui::ColorEdit3("Horizon Color", horizonColorArray))
        {
            skyPass->SetSkyHorizonColor(Vec3(horizonColorArray[0], horizonColorArray[1], horizonColorArray[2]));
        }

        // Help text for horizon color
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Color at the horizon (default: light blue)");
        }

        ImGui::Separator();

        // ==================== Read-Only Info Display ====================

        ImGui::Text("Celestial Bodies:");
        ImGui::Spacing();

        // Sun Position (read-only)
        // [NOTE] Sun/Moon positions are calculated in Execute(), not accessible here
        // This is a design limitation - positions are computed per-frame in Execute()
        ImGui::BulletText("Sun Position: Calculated per-frame");
        ImGui::BulletText("Moon Position: Calculated per-frame");

        // Help text
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Sun/Moon positions are computed dynamically based on celestial angle");
        }

        // ==================== Reset to Defaults ====================

        ImGui::Separator();

        if (ImGui::Button("Reset to Defaults"))
        {
            skyPass->SetVoidGradientEnabled(true);
            skyPass->SetSkyZenithColor(Vec3(0.47f, 0.65f, 1.0f)); // Default blue
            skyPass->SetSkyHorizonColor(Vec3(0.75f, 0.85f, 1.0f)); // Default light blue
        }

        // Help text for reset button
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Reset all sky parameters to Minecraft vanilla defaults");
        }

        ImGui::Unindent();
    }
}
