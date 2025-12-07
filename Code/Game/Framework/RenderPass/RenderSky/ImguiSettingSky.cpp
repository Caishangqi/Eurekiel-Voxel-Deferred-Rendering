#include "ImguiSettingSky.hpp"
#include "SkyRenderPass.hpp"
#include "SkyColorHelper.hpp"
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

        // ==================== Sky Phase Colors (4-phase system) ====================

        if (ImGui::TreeNode("Sky Dome Phase Colors"))
        {
            ImGui::TextDisabled("(?) 4-phase interpolation system for sky dome");
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip(
                    "[IMPORTANT] These are rendering phase color names, not Minecraft time points!\n"
                    "Phase 0: Sunrise Color (interpolated around dawn)\n"
                    "Phase 1: Noon Color (interpolated around midday)\n"
                    "Phase 2: Sunset Color (interpolated around dusk)\n"
                    "Phase 3: Midnight Color (interpolated at night)\n"
                    "\n"
                    "Minecraft official time points (TimeCommand.java:17-24):\n"
                    "- day = 1000 ticks (6:00 AM)\n"
                    "- noon = 6000 ticks (12:00 PM)\n"
                    "- night = 13000 ticks (7:00 PM, night starts)\n"
                    "- midnight = 18000 ticks (12:00 AM)");
            }

            SkyPhaseColors& skyColors = SkyColorHelper::GetSkyColors();

            float sunriseColor[3] = {skyColors.day.x, skyColors.day.y, skyColors.day.z};
            if (ImGui::ColorEdit3("Sunrise##Sky", sunriseColor))
            {
                skyColors.day = Vec3(sunriseColor[0], sunriseColor[1], sunriseColor[2]);
            }

            float noonColor[3] = {skyColors.noon.x, skyColors.noon.y, skyColors.noon.z};
            if (ImGui::ColorEdit3("Noon##Sky", noonColor))
            {
                skyColors.noon = Vec3(noonColor[0], noonColor[1], noonColor[2]);
            }

            float sunsetColor[3] = {skyColors.night.x, skyColors.night.y, skyColors.night.z};
            if (ImGui::ColorEdit3("Sunset##Sky", sunsetColor))
            {
                skyColors.night = Vec3(sunsetColor[0], sunsetColor[1], sunsetColor[2]);
            }

            float midnightColor[3] = {skyColors.midnight.x, skyColors.midnight.y, skyColors.midnight.z};
            if (ImGui::ColorEdit3("Midnight##Sky", midnightColor))
            {
                skyColors.midnight = Vec3(midnightColor[0], midnightColor[1], midnightColor[2]);
            }

            if (ImGui::Button("Reset Sky Colors"))
            {
                SkyColorHelper::ResetSkyColorsToDefault();
            }

            ImGui::TreePop();
        }

        // ==================== Fog Phase Colors (4-phase system) ====================

        if (ImGui::TreeNode("Fog Phase Colors"))
        {
            ImGui::TextDisabled("(?) Fog colors for Clear RT (lighter than sky)");
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip(
                    "Fog colors are used for clearing the render target.\n"
                    "Generally lighter/more desaturated than sky colors.");
            }

            SkyPhaseColors& fogColors = SkyColorHelper::GetFogColors();

            float fogSunrise[3] = {fogColors.day.x, fogColors.day.y, fogColors.day.z};
            if (ImGui::ColorEdit3("Sunrise##Fog", fogSunrise))
            {
                fogColors.day = Vec3(fogSunrise[0], fogSunrise[1], fogSunrise[2]);
            }

            float fogNoon[3] = {fogColors.noon.x, fogColors.noon.y, fogColors.noon.z};
            if (ImGui::ColorEdit3("Noon##Fog", fogNoon))
            {
                fogColors.noon = Vec3(fogNoon[0], fogNoon[1], fogNoon[2]);
            }

            float fogSunset[3] = {fogColors.night.x, fogColors.night.y, fogColors.night.z};
            if (ImGui::ColorEdit3("Sunset##Fog", fogSunset))
            {
                fogColors.night = Vec3(fogSunset[0], fogSunset[1], fogSunset[2]);
            }

            float fogMidnight[3] = {fogColors.midnight.x, fogColors.midnight.y, fogColors.midnight.z};
            if (ImGui::ColorEdit3("Midnight##Fog", fogMidnight))
            {
                fogColors.midnight = Vec3(fogMidnight[0], fogMidnight[1], fogMidnight[2]);
            }

            if (ImGui::Button("Reset Fog Colors"))
            {
                SkyColorHelper::ResetFogColorsToDefault();
            }

            ImGui::TreePop();
        }

        ImGui::Separator();

        // ==================== Legacy Sky Color Adjustment ====================

        if (ImGui::TreeNode("Legacy Sky Colors (Deprecated)"))
        {
            ImGui::TextDisabled("(?) Old zenith/horizon system - use Phase Colors instead");

            Vec3  zenithColor         = skyPass->GetSkyZenithColor();
            float zenithColorArray[3] = {zenithColor.x, zenithColor.y, zenithColor.z};

            if (ImGui::ColorEdit3("Zenith Color", zenithColorArray))
            {
                skyPass->SetSkyZenithColor(Vec3(zenithColorArray[0], zenithColorArray[1], zenithColorArray[2]));
            }

            Vec3  horizonColor         = skyPass->GetSkyHorizonColor();
            float horizonColorArray[3] = {horizonColor.x, horizonColor.y, horizonColor.z};

            if (ImGui::ColorEdit3("Horizon Color", horizonColorArray))
            {
                skyPass->SetSkyHorizonColor(Vec3(horizonColorArray[0], horizonColorArray[1], horizonColorArray[2]));
            }

            ImGui::TreePop();
        }

        ImGui::Separator();

        // ==================== Reset All to Defaults ====================

        if (ImGui::Button("Reset All to Defaults"))
        {
            skyPass->SetVoidGradientEnabled(true);
            skyPass->SetSkyZenithColor(Vec3(0.47f, 0.65f, 1.0f));
            skyPass->SetSkyHorizonColor(Vec3(0.75f, 0.85f, 1.0f));
            SkyColorHelper::ResetSkyColorsToDefault();
            SkyColorHelper::ResetFogColorsToDefault();
        }

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Reset all sky parameters including phase colors to defaults");
        }

        ImGui::Unindent();
    }
}
