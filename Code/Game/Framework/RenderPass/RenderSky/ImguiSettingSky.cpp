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

        // ==================== Sunrise/Sunset Strip Colors ====================

        if (ImGui::TreeNode("Sunrise/Sunset Strip Colors"))
        {
            ImGui::TextDisabled("(?) Strip glow colors at horizon during sunrise/sunset");
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip(
                    "The sunrise strip is the horizontal glow band at the horizon.\n"
                    "This is SEPARATE from the sky dome colors - the strip overlays the sky.\n"
                    "\n"
                    "Sunrise Strip: Visible around tick 23000-1000 (early morning)\n"
                    "Sunset Strip: Visible around tick 11000-13000 (evening)");
            }

            SunriseStripColors& stripColors = SkyColorHelper::GetStripColors();

            float sunriseStrip[3] = {stripColors.sunriseStrip.x, stripColors.sunriseStrip.y, stripColors.sunriseStrip.z};
            if (ImGui::ColorEdit3("Sunrise Strip##Strip", sunriseStrip))
            {
                stripColors.sunriseStrip = Vec3(sunriseStrip[0], sunriseStrip[1], sunriseStrip[2]);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Strip color during sunrise (tick 23000-1000).\nDefault: warm orange-yellow.");
            }

            float sunsetStrip[3] = {stripColors.sunsetStrip.x, stripColors.sunsetStrip.y, stripColors.sunsetStrip.z};
            if (ImGui::ColorEdit3("Sunset Strip##Strip", sunsetStrip))
            {
                stripColors.sunsetStrip = Vec3(sunsetStrip[0], sunsetStrip[1], sunsetStrip[2]);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Strip color during sunset (tick 11000-13000).\nDefault: orange-red.");
            }

            if (ImGui::Button("Reset Strip Colors"))
            {
                SkyColorHelper::ResetStripColorsToDefault();
            }

            ImGui::TreePop();
        }

        // ==================== Phase Transition Easing (Bezier Curves) ====================

        if (ImGui::TreeNode("Phase Transition Easing"))
        {
            ImGui::TextDisabled("(?) Bezier curves for non-linear color transitions");
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip(
                    "Controls how quickly colors transition between phases.\n"
                    "Like CSS cubic-bezier: maps linear time to eased time.\n"
                    "\n"
                    "Control Points P1(x1,y1) and P2(x2,y2):\n"
                    "- Linear: P1(0,0) P2(1,1) - constant speed\n"
                    "- EaseIn: P1(0.42,0) P2(1,1) - slow start\n"
                    "- EaseOut: P1(0,0) P2(0.58,1) - slow end\n"
                    "- HoldStart: P1(0.8,0) P2(0.9,0.1) - stay at start longer\n"
                    "\n"
                    "Use 'Minecraft Style' for longer nights!");
            }

            SkyEasingConfig& easing = SkyColorHelper::GetEasingConfig();

            // Helper lambda for editing a single Bezier easing
            auto EditBezierEasing = [](const char* label, BezierEasing& bez) -> bool
            {
                bool changed = false;
                if (ImGui::TreeNode(label))
                {
                    // P1 control point
                    float p1[2] = {bez.p1.x, bez.p1.y};
                    if (ImGui::SliderFloat2("P1 (x1, y1)", p1, 0.0f, 1.0f, "%.2f"))
                    {
                        bez.p1.x = p1[0];
                        bez.p1.y = p1[1];
                        changed  = true;
                    }
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip("First control point.\nHigher y1 = faster start.");
                    }

                    // P2 control point
                    float p2[2] = {bez.p2.x, bez.p2.y};
                    if (ImGui::SliderFloat2("P2 (x2, y2)", p2, 0.0f, 1.0f, "%.2f"))
                    {
                        bez.p2.x = p2[0];
                        bez.p2.y = p2[1];
                        changed  = true;
                    }
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip("Second control point.\nLower x2 = slower end.");
                    }

                    // Quick preset buttons
                    if (ImGui::Button("Linear"))
                    {
                        bez     = BezierEasing::Linear();
                        changed = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("EaseIn"))
                    {
                        bez     = BezierEasing::EaseIn();
                        changed = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("EaseOut"))
                    {
                        bez     = BezierEasing::EaseOut();
                        changed = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("EaseInOut"))
                    {
                        bez     = BezierEasing::EaseInOut();
                        changed = true;
                    }

                    if (ImGui::Button("HoldStart"))
                    {
                        bez     = BezierEasing::HoldStart();
                        changed = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("HoldEnd"))
                    {
                        bez     = BezierEasing::HoldEnd();
                        changed = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("HoldMiddle"))
                    {
                        bez     = BezierEasing::HoldMiddle();
                        changed = true;
                    }

                    ImGui::TreePop();
                }
                return changed;
            };

            // Phase transition easings
            EditBezierEasing("Noon -> Sunset (Phase 0)", easing.noonToSunset);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("celestialAngle 0.0 - 0.25\nDay to evening transition.");
            }

            EditBezierEasing("Sunset -> Midnight (Phase 1)", easing.sunsetToMidnight);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("celestialAngle 0.25 - 0.5\nEvening to night. Use HoldEnd for longer night!");
            }

            EditBezierEasing("Midnight -> Sunrise (Phase 2)", easing.midnightToSunrise);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("celestialAngle 0.5 - 0.75\nNight to morning. Use HoldStart for longer night!");
            }

            EditBezierEasing("Sunrise -> Dawn (Phase 3)", easing.sunriseToDawn);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("celestialAngle 0.75 - 0.79\nSunrise glow to dawn (tick 0-1000).");
            }

            EditBezierEasing("Dawn -> Noon (Phase 4)", easing.dawnToNoon);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("celestialAngle 0.79 - 1.0\nMorning to midday.");
            }

            ImGui::Separator();

            // Global preset buttons
            if (ImGui::Button("All Linear (Default)"))
            {
                SkyColorHelper::ResetEasingToDefault();
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Reset all phases to linear interpolation.\nFast, uniform transitions.");
            }

            ImGui::SameLine();
            if (ImGui::Button("Minecraft Style"))
            {
                SkyColorHelper::SetMinecraftStyleEasing();
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Longer nights with quick sunrise/sunset.\nPhase 1: quick to midnight, hold\nPhase 2: hold midnight, quick sunrise");
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
            SkyColorHelper::ResetStripColorsToDefault();
            SkyColorHelper::ResetEasingToDefault();
        }

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Reset all sky parameters including phase colors, strip colors, and easing to defaults");
        }

        ImGui::Unindent();
    }
}
