/**
 * @file ImguiSettingComposite.cpp
 * @brief Static ImGui interface for CompositeRenderPass configuration
 */

#include "ImguiSettingComposite.hpp"
#include "Game/Gameplay/Game.hpp"
#include "ThirdParty/imgui/imgui.h"

// Fog mode constants (matches HLSL fog.hlsl)
static constexpr int FOG_MODE_OFF    = 0;
static constexpr int FOG_MODE_LINEAR = 9729;
static constexpr int FOG_MODE_EXP    = 2048;
static constexpr int FOG_MODE_EXP2   = 2049;

void ImguiSettingComposite::Show(CompositeRenderPass* compositePass)
{
    (void)compositePass; // Reserved for future use

    if (ImGui::CollapsingHeader("Composite Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Indent();

        ImGui::Text("Underwater Fog Effect");
        ImGui::Separator();

        // ==================== Fog Color ====================
        float fogColorArr[3] = {FOG_UNIFORM.fogColor.x, FOG_UNIFORM.fogColor.y, FOG_UNIFORM.fogColor.z};
        if (ImGui::ColorEdit3("Fog Color", fogColorArr))
        {
            FOG_UNIFORM.fogColor = Vec3(fogColorArr[0], fogColorArr[1], fogColorArr[2]);
        }

        // ==================== Fog Start ====================
        ImGui::SliderFloat("Fog Start", &FOG_UNIFORM.fogStart, 0.0f, 10.0f, "%.2f");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Linear fog start distance (where fog begins)");
        }

        // ==================== Fog End ====================
        ImGui::SliderFloat("Fog End", &FOG_UNIFORM.fogEnd, 10.0f, 200.0f, "%.1f");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Linear fog end distance (where fog is fully opaque)");
        }

        // ==================== Fog Density ====================
        ImGui::SliderFloat("Fog Density", &FOG_UNIFORM.fogDensity, 0.0f, 1.0f, "%.3f");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Fog density for Exp/Exp2 modes");
        }

        // ==================== Fog Mode ====================
        const char* fogModes[]  = {"Off", "Linear", "Exponential", "Exponential Squared"};
        int         currentMode = 2;
        if (FOG_UNIFORM.fogMode == FOG_MODE_LINEAR)
            currentMode = 1;
        else if (FOG_UNIFORM.fogMode == FOG_MODE_EXP)
            currentMode = 2;
        else if (FOG_UNIFORM.fogMode == FOG_MODE_EXP2)
            currentMode = 3;

        if (ImGui::Combo("Fog Mode", &currentMode, fogModes, IM_ARRAYSIZE(fogModes)))
        {
            switch (currentMode)
            {
            case 0:
                FOG_UNIFORM.fogMode = FOG_MODE_OFF;
                break;
            case 1:
                FOG_UNIFORM.fogMode = FOG_MODE_LINEAR;
                break;
            case 2:
                FOG_UNIFORM.fogMode = FOG_MODE_EXP;
                break;
            case 3:
                FOG_UNIFORM.fogMode = FOG_MODE_EXP2;
                break;
            }
        }

        ImGui::Spacing();
        ImGui::Separator();

        // ==================== Debug Info ====================
        if (ImGui::TreeNode("Debug Info"))
        {
            const char* eyeStateNames[] = {"Air", "Water", "Lava", "Powder Snow"};
            int         eyeState        = COMMON_UNIFORM.isEyeInWater;
            if (eyeState >= 0 && eyeState <= 3)
            {
                ImGui::Text("Eye State: %s", eyeStateNames[eyeState]);
            }
            else
            {
                ImGui::Text("Eye State: Unknown (%d)", eyeState);
            }

            ImGui::Spacing();
            ImGui::Text("Current Fog Parameters:");
            ImGui::BulletText("Color: (%.2f, %.2f, %.2f)", FOG_UNIFORM.fogColor.x, FOG_UNIFORM.fogColor.y,
                              FOG_UNIFORM.fogColor.z);
            ImGui::BulletText("Start: %.2f", FOG_UNIFORM.fogStart);
            ImGui::BulletText("End: %.2f", FOG_UNIFORM.fogEnd);
            ImGui::BulletText("Density: %.3f", FOG_UNIFORM.fogDensity);
            ImGui::BulletText("Mode: %d", FOG_UNIFORM.fogMode);
            ImGui::BulletText("Shape: %d", FOG_UNIFORM.fogShape);

            ImGui::TreePop();
        }

        ImGui::Unindent();
    }
}
