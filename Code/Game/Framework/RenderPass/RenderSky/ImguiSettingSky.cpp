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

        // ==================== Sky Phase Colors (5-phase system) ====================

        if (ImGui::TreeNode("Sky Dome Phase Colors"))
        {
            ImGui::TextDisabled("(?) 5-phase interpolation system for sky dome");
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip(
                    "[NEW] 5-phase system with Dawn phase for tick 0-1000 transition!\n"
                    "\n"
                    "Phase 0: Sunrise Color (tick 0, 6:00 AM) - Orange-pink\n"
                    "Phase 1: Dawn Color (tick 1000, 7:00 AM) - Light yellow [NEW]\n"
                    "Phase 2: Noon Color (tick 6000, 12:00 PM) - Bright blue\n"
                    "Phase 3: Sunset Color (tick 12000, 6:00 PM) - Orange-red\n"
                    "Phase 4: Midnight Color (tick 18000, 12:00 AM) - Dark blue/black\n"
                    "\n"
                    "Minecraft official time points (TimeCommand.java:17-24):\n"
                    "- day = 1000 ticks (7:00 AM, dawn complete)\n"
                    "- noon = 6000 ticks (12:00 PM)\n"
                    "- night = 13000 ticks (7:00 PM, night starts)\n"
                    "- midnight = 18000 ticks (12:00 AM)");
            }

            SkyPhaseColors& skyColors = SkyColorHelper::GetSkyColors();

            float sunriseColor[3] = {skyColors.sunrise.x, skyColors.sunrise.y, skyColors.sunrise.z};
            if (ImGui::ColorEdit3("Sunrise (tick 0)##Sky", sunriseColor))
            {
                skyColors.sunrise = Vec3(sunriseColor[0], sunriseColor[1], sunriseColor[2]);
            }

            // [NEW] Dawn phase color editor
            float dawnColor[3] = {skyColors.dawn.x, skyColors.dawn.y, skyColors.dawn.z};
            if (ImGui::ColorEdit3("Dawn (tick 1000)##Sky", dawnColor))
            {
                skyColors.dawn = Vec3(dawnColor[0], dawnColor[1], dawnColor[2]);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Light yellow morning sky after sunrise.\nTransition from orange sunrise to bright day.");
            }

            float noonColor[3] = {skyColors.noon.x, skyColors.noon.y, skyColors.noon.z};
            if (ImGui::ColorEdit3("Noon (tick 6000)##Sky", noonColor))
            {
                skyColors.noon = Vec3(noonColor[0], noonColor[1], noonColor[2]);
            }

            float sunsetColor[3] = {skyColors.sunset.x, skyColors.sunset.y, skyColors.sunset.z};
            if (ImGui::ColorEdit3("Sunset (tick 12000)##Sky", sunsetColor))
            {
                skyColors.sunset = Vec3(sunsetColor[0], sunsetColor[1], sunsetColor[2]);
            }

            float midnightColor[3] = {skyColors.midnight.x, skyColors.midnight.y, skyColors.midnight.z};
            if (ImGui::ColorEdit3("Midnight (tick 18000)##Sky", midnightColor))
            {
                skyColors.midnight = Vec3(midnightColor[0], midnightColor[1], midnightColor[2]);
            }

            if (ImGui::Button("Reset Sky Colors"))
            {
                SkyColorHelper::ResetSkyColorsToDefault();
            }

            ImGui::TreePop();
        }

        // ==================== Fog Phase Colors (5-phase system) ====================

        if (ImGui::TreeNode("Fog Phase Colors"))
        {
            ImGui::TextDisabled("(?) 5-phase fog colors for Clear RT (lighter than sky)");
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip(
                    "Fog colors are used for clearing the render target.\n"
                    "Generally lighter/more desaturated than sky colors.\n"
                    "[NEW] Includes Dawn phase for tick 0-1000 transition.");
            }

            SkyPhaseColors& fogColors = SkyColorHelper::GetFogColors();

            float fogSunrise[3] = {fogColors.sunrise.x, fogColors.sunrise.y, fogColors.sunrise.z};
            if (ImGui::ColorEdit3("Sunrise (tick 0)##Fog", fogSunrise))
            {
                fogColors.sunrise = Vec3(fogSunrise[0], fogSunrise[1], fogSunrise[2]);
            }

            // [NEW] Dawn phase fog color editor
            float fogDawn[3] = {fogColors.dawn.x, fogColors.dawn.y, fogColors.dawn.z};
            if (ImGui::ColorEdit3("Dawn (tick 1000)##Fog", fogDawn))
            {
                fogColors.dawn = Vec3(fogDawn[0], fogDawn[1], fogDawn[2]);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Light morning fog after sunrise.\nTransition from warm sunrise fog to bright day fog.");
            }

            float fogNoon[3] = {fogColors.noon.x, fogColors.noon.y, fogColors.noon.z};
            if (ImGui::ColorEdit3("Noon (tick 6000)##Fog", fogNoon))
            {
                fogColors.noon = Vec3(fogNoon[0], fogNoon[1], fogNoon[2]);
            }

            float fogSunset[3] = {fogColors.sunset.x, fogColors.sunset.y, fogColors.sunset.z};
            if (ImGui::ColorEdit3("Sunset (tick 12000)##Fog", fogSunset))
            {
                fogColors.sunset = Vec3(fogSunset[0], fogSunset[1], fogSunset[2]);
            }

            float fogMidnight[3] = {fogColors.midnight.x, fogColors.midnight.y, fogColors.midnight.z};
            if (ImGui::ColorEdit3("Midnight (tick 18000)##Fog", fogMidnight))
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
