/**
 * @file ImguiSceneRendering.cpp
 * @brief ImGui Scene Rendering Tab Implementation
 * @date 2025-11-26
 */

#include "ImguiSceneRendering.hpp"

#include "Game/GameCommon.hpp"
#include "Game/Framework/RenderPass/RenderCloud/ImguiSettingCloud.hpp"
#include "Game/Framework/RenderPass/RenderCloud/CloudRenderPass.hpp"
#include "Game/Framework/RenderPass/RenderSky/ImguiSettingSky.hpp"
#include "Game/Framework/RenderPass/RenderSky/SkyRenderPass.hpp"
#include "Game/Gameplay/Game.hpp"
#include "ThirdParty/imgui/imgui.h"

/**
 * @brief Show Scene Rendering tab content
 */
void ImguiSceneRendering::Show()
{
    // [MODULE 1] Sky Rendering
    // Note: ImguiSettingSky::Show() requires SkyRenderPass* parameter
    // This will be integrated in Task 18 when connecting to Game::RenderImGui()
    // For now, we create the UI structure

    // [MODULE 2] Cloud Rendering
    CloudRenderPass* cloudPass = dynamic_cast<CloudRenderPass*>(g_theGame->m_cloudRenderPass.get());
    if (cloudPass)
    {
        ImguiSettingCloud::Show(cloudPass);
    }

    // [MODULE 3] Sun & Moon Parameters
    ShowSunMoonParameters(); /// WRONG should be ImguiSettingSunMoon::Show()
    ImguiSettingSky::Show(dynamic_cast<SkyRenderPass*>(g_theGame->m_skyRenderPass.get()));

    // [FUTURE] Add more rendering modules here
    // - Terrain Rendering
    // - Water Rendering
    // - Post-Processing Effects
}

/**
 * @brief Show Sun & Moon parameters section
 */
void ImguiSceneRendering::ShowSunMoonParameters()
{
    if (ImGui::CollapsingHeader("Sun & Moon"))
    {
        ImGui::Indent();

        // [PARAMETER 1] Sun Size
        static float sunSize = 30.0f; // Default billboard size
        if (ImGui::SliderFloat("Sun Size", &sunSize, 10.0f, 100.0f, "%.1f"))
        {
            // [TODO] Task 18: Apply to SkyRenderPass
            // skyPass->SetSunSize(sunSize);
        }

        // [PARAMETER 2] Moon Size
        static float moonSize = 30.0f; // Default billboard size
        if (ImGui::SliderFloat("Moon Size", &moonSize, 10.0f, 100.0f, "%.1f"))
        {
            // [TODO] Task 18: Apply to SkyRenderPass
            // skyPass->SetMoonSize(moonSize);
        }

        // [PARAMETER 3] Celestial Brightness
        static float celestialBrightness = 1.0f;
        if (ImGui::SliderFloat("Celestial Brightness", &celestialBrightness, 0.0f, 2.0f, "%.2f"))
        {
            // [TODO] Task 18: Apply to SkyRenderPass
            // skyPass->SetCelestialBrightness(celestialBrightness);
        }

        // [BUTTON] Reset to Defaults
        if (ImGui::Button("Reset Sun & Moon"))
        {
            sunSize             = 30.0f;
            moonSize            = 30.0f;
            celestialBrightness = 1.0f;
            // [TODO] Task 18: Apply defaults to SkyRenderPass
        }

        ImGui::Unindent();
    }
}
