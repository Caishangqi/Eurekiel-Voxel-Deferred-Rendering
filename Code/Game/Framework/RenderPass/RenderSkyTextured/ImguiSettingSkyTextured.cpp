#include "ImguiSettingSkyTextured.hpp"
#include "SkyTexturedRenderPass.hpp"
#include "ThirdParty/imgui/imgui.h"

void ImguiSettingSkyTextured::Show(SkyTexturedRenderPass* skyPass)
{
    if (!skyPass)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "[ERROR] SkyTexturedRenderPass is null");
        return;
    }

    if (ImGui::CollapsingHeader("Sky Textured Rendering"))
    {
        ImGui::Indent();

        // ==================== Celestial Body Size ====================

        if (ImGui::TreeNode("Celestial Body Size"))
        {
            ImGui::TextDisabled("(?) Sun and Moon billboard sizes");

            float sunSize = skyPass->GetSunSize();
            if (ImGui::SliderFloat("Sun Size", &sunSize, 5.0f, 100.0f, "%.1f"))
                skyPass->SetSunSize(sunSize);

            float moonSize = skyPass->GetMoonSize();
            if (ImGui::SliderFloat("Moon Size", &moonSize, 5.0f, 100.0f, "%.1f"))
                skyPass->SetMoonSize(moonSize);

            if (ImGui::Button("Reset to Defaults##CelestialSize"))
            {
                skyPass->SetSunSize(30.0f);
                skyPass->SetMoonSize(20.0f);
            }

            ImGui::TreePop();
        }

        ImGui::Separator();

        // ==================== Star Rendering ====================

        if (ImGui::TreeNode("Star Rendering"))
        {
            ImGui::TextDisabled("(?) Star field rendering parameters");

            bool enableStars = skyPass->IsStarRenderingEnabled();
            if (ImGui::Checkbox("Enable Star Rendering", &enableStars))
                skyPass->SetStarRenderingEnabled(enableStars);

            float brightness = skyPass->GetStarBrightnessMultiplier();
            if (ImGui::SliderFloat("Brightness Multiplier", &brightness, 0.0f, 3.0f, "%.2f"))
                skyPass->SetStarBrightnessMultiplier(brightness);

            int starSeed = static_cast<int>(skyPass->GetStarSeed());
            if (ImGui::InputInt("Random Seed", &starSeed))
            {
                if (starSeed >= 0)
                    skyPass->SetStarSeed(static_cast<unsigned int>(starSeed));
            }

            if (ImGui::Button("Reset to Defaults##Stars"))
            {
                skyPass->SetStarRenderingEnabled(true);
                skyPass->SetStarBrightnessMultiplier(1.0f);
                skyPass->SetStarSeed(10842);
            }

            ImGui::TreePop();
        }

        ImGui::Separator();

        // ==================== Reset All ====================

        if (ImGui::Button("Reset All to Defaults##SkyTextured"))
        {
            skyPass->SetSunSize(30.0f);
            skyPass->SetMoonSize(20.0f);
            skyPass->SetStarRenderingEnabled(true);
            skyPass->SetStarBrightnessMultiplier(1.0f);
            skyPass->SetStarSeed(10842);
        }

        ImGui::Unindent();
    }
}
