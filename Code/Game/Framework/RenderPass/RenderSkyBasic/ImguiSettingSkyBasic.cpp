#include "ImguiSettingSkyBasic.hpp"
#include "SkyBasicRenderPass.hpp"
#include "SkyColorHelper.hpp"
#include "Engine/Math/Vec3.hpp"
#include "ThirdParty/imgui/imgui.h"

void ImguiSettingSkyBasic::Show(SkyBasicRenderPass* skyPass)
{
    if (!skyPass)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "[ERROR] SkyBasicRenderPass is null");
        return;
    }

    if (ImGui::CollapsingHeader("Sky Basic Rendering"))
    {
        ImGui::Indent();

        // ==================== Void Gradient Toggle ====================

        bool enableVoidGradient = skyPass->IsVoidGradientEnabled();
        if (ImGui::Checkbox("Enable Void Gradient", &enableVoidGradient))
        {
            skyPass->SetVoidGradientEnabled(enableVoidGradient);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Darkens sky when camera is below Y=-64 (Minecraft style)");
        }

        ImGui::Separator();

        // ==================== Sky Phase Colors (5-phase system) ====================

        if (ImGui::TreeNode("Sky Dome Phase Colors"))
        {
            ImGui::TextDisabled("(?) 5-phase interpolation system for sky dome");

            SkyPhaseColors& skyColors = SkyColorHelper::GetSkyColors();

            float sunriseColor[3] = {skyColors.sunrise.x, skyColors.sunrise.y, skyColors.sunrise.z};
            if (ImGui::ColorEdit3("Sunrise (tick 0)##Sky", sunriseColor))
                skyColors.sunrise = Vec3(sunriseColor[0], sunriseColor[1], sunriseColor[2]);

            float dawnColor[3] = {skyColors.dawn.x, skyColors.dawn.y, skyColors.dawn.z};
            if (ImGui::ColorEdit3("Dawn (tick 1000)##Sky", dawnColor))
                skyColors.dawn = Vec3(dawnColor[0], dawnColor[1], dawnColor[2]);

            float noonColor[3] = {skyColors.noon.x, skyColors.noon.y, skyColors.noon.z};
            if (ImGui::ColorEdit3("Noon (tick 6000)##Sky", noonColor))
                skyColors.noon = Vec3(noonColor[0], noonColor[1], noonColor[2]);

            float sunsetColor[3] = {skyColors.sunset.x, skyColors.sunset.y, skyColors.sunset.z};
            if (ImGui::ColorEdit3("Sunset (tick 12000)##Sky", sunsetColor))
                skyColors.sunset = Vec3(sunsetColor[0], sunsetColor[1], sunsetColor[2]);

            float midnightColor[3] = {skyColors.midnight.x, skyColors.midnight.y, skyColors.midnight.z};
            if (ImGui::ColorEdit3("Midnight (tick 18000)##Sky", midnightColor))
                skyColors.midnight = Vec3(midnightColor[0], midnightColor[1], midnightColor[2]);

            if (ImGui::Button("Reset Sky Colors"))
                SkyColorHelper::ResetSkyColorsToDefault();

            ImGui::TreePop();
        }

        // ==================== Fog Phase Colors ====================

        if (ImGui::TreeNode("Fog Phase Colors"))
        {
            ImGui::TextDisabled("(?) 5-phase fog colors for Clear RT");

            SkyPhaseColors& fogColors = SkyColorHelper::GetFogColors();

            float fogSunrise[3] = {fogColors.sunrise.x, fogColors.sunrise.y, fogColors.sunrise.z};
            if (ImGui::ColorEdit3("Sunrise (tick 0)##Fog", fogSunrise))
                fogColors.sunrise = Vec3(fogSunrise[0], fogSunrise[1], fogSunrise[2]);

            float fogDawn[3] = {fogColors.dawn.x, fogColors.dawn.y, fogColors.dawn.z};
            if (ImGui::ColorEdit3("Dawn (tick 1000)##Fog", fogDawn))
                fogColors.dawn = Vec3(fogDawn[0], fogDawn[1], fogDawn[2]);

            float fogNoon[3] = {fogColors.noon.x, fogColors.noon.y, fogColors.noon.z};
            if (ImGui::ColorEdit3("Noon (tick 6000)##Fog", fogNoon))
                fogColors.noon = Vec3(fogNoon[0], fogNoon[1], fogNoon[2]);

            float fogSunset[3] = {fogColors.sunset.x, fogColors.sunset.y, fogColors.sunset.z};
            if (ImGui::ColorEdit3("Sunset (tick 12000)##Fog", fogSunset))
                fogColors.sunset = Vec3(fogSunset[0], fogSunset[1], fogSunset[2]);

            float fogMidnight[3] = {fogColors.midnight.x, fogColors.midnight.y, fogColors.midnight.z};
            if (ImGui::ColorEdit3("Midnight (tick 18000)##Fog", fogMidnight))
                fogColors.midnight = Vec3(fogMidnight[0], fogMidnight[1], fogMidnight[2]);

            if (ImGui::Button("Reset Fog Colors"))
                SkyColorHelper::ResetFogColorsToDefault();

            ImGui::TreePop();
        }

        // ==================== Sunrise/Sunset Strip Colors ====================

        if (ImGui::TreeNode("Sunrise/Sunset Strip Colors"))
        {
            ImGui::TextDisabled("(?) Strip glow colors at horizon during sunrise/sunset");

            SunriseStripColors& stripColors = SkyColorHelper::GetStripColors();

            float sunriseStrip[3] = {stripColors.sunriseStrip.x, stripColors.sunriseStrip.y, stripColors.sunriseStrip.z};
            if (ImGui::ColorEdit3("Sunrise Strip##Strip", sunriseStrip))
                stripColors.sunriseStrip = Vec3(sunriseStrip[0], sunriseStrip[1], sunriseStrip[2]);

            float sunsetStrip[3] = {stripColors.sunsetStrip.x, stripColors.sunsetStrip.y, stripColors.sunsetStrip.z};
            if (ImGui::ColorEdit3("Sunset Strip##Strip", sunsetStrip))
                stripColors.sunsetStrip = Vec3(sunsetStrip[0], sunsetStrip[1], sunsetStrip[2]);

            if (ImGui::Button("Reset Strip Colors"))
                SkyColorHelper::ResetStripColorsToDefault();

            ImGui::TreePop();
        }

        // ==================== Phase Transition Easing ====================

        if (ImGui::TreeNode("Phase Transition Easing"))
        {
            ImGui::TextDisabled("(?) Bezier curves for non-linear color transitions");

            SkyEasingConfig& easing = SkyColorHelper::GetEasingConfig();

            auto EditBezierEasing = [](const char* label, BezierEasing& bez) -> bool
            {
                bool changed = false;
                if (ImGui::TreeNode(label))
                {
                    float p1[2] = {bez.p1.x, bez.p1.y};
                    if (ImGui::SliderFloat2("P1 (x1, y1)", p1, 0.0f, 1.0f, "%.2f"))
                    {
                        bez.p1.x = p1[0];
                        bez.p1.y = p1[1];
                        changed  = true;
                    }
                    float p2[2] = {bez.p2.x, bez.p2.y};
                    if (ImGui::SliderFloat2("P2 (x2, y2)", p2, 0.0f, 1.0f, "%.2f"))
                    {
                        bez.p2.x = p2[0];
                        bez.p2.y = p2[1];
                        changed  = true;
                    }
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

            EditBezierEasing("Noon -> Sunset (Phase 0)", easing.noonToSunset);
            EditBezierEasing("Sunset -> Midnight (Phase 1)", easing.sunsetToMidnight);
            EditBezierEasing("Midnight -> Sunrise (Phase 2)", easing.midnightToSunrise);
            EditBezierEasing("Sunrise -> Dawn (Phase 3)", easing.sunriseToDawn);
            EditBezierEasing("Dawn -> Noon (Phase 4)", easing.dawnToNoon);

            ImGui::Separator();
            if (ImGui::Button("All Linear (Default)"))
                SkyColorHelper::ResetEasingToDefault();
            ImGui::SameLine();
            if (ImGui::Button("Minecraft Style"))
                SkyColorHelper::SetMinecraftStyleEasing();

            ImGui::TreePop();
        }

        ImGui::Separator();

        // ==================== Reset All ====================

        if (ImGui::Button("Reset All to Defaults##SkyBasic"))
        {
            skyPass->SetVoidGradientEnabled(true);
            skyPass->SetSkyZenithColor(Vec3(0.47f, 0.65f, 1.0f));
            skyPass->SetSkyHorizonColor(Vec3(0.75f, 0.85f, 1.0f));
            SkyColorHelper::ResetSkyColorsToDefault();
            SkyColorHelper::ResetFogColorsToDefault();
            SkyColorHelper::ResetStripColorsToDefault();
            SkyColorHelper::ResetEasingToDefault();
        }

        ImGui::Unindent();
    }
}
