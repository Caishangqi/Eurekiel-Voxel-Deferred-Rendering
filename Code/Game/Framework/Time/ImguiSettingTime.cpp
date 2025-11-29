#include "ImguiSettingTime.hpp"
#include "TimeOfDayManager.hpp"
#include "Engine/Core/StringUtils.hpp"
#include "ThirdParty/imgui/imgui.h"

/**
 * Show - Render time system debug UI
 *
 * Implementation Notes:
 * - Uses ImGui::CollapsingHeader for collapsible section
 * - ImGui::Text for read-only parameter display
 * - Future: Add SliderInt/SliderFloat for interactive debugging
 *
 * Reference:
 * - imgui_demo.cpp:434 (CollapsingHeader example)
 * - imgui_demo.cpp:955 (SliderInt example)
 */
void ImguiSettingTime::Show(TimeOfDayManager* timeManager)
{
    if (!timeManager)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "[ERROR] TimeOfDayManager is null");
        return;
    }

    // CollapsingHeader: Collapsible section for time system parameters
    // Returns true if expanded, false if collapsed
    if (ImGui::CollapsingHeader("Time System"))
    {
        // Indent content for better visual hierarchy
        ImGui::Indent();

        // ==================== Read-Only Parameter Display ====================
        ImGui::SeparatorText("Time Info");
        // Current Tick (0-23999)
        int currentTick = timeManager->GetCurrentTick();
        ImGui::Text("Current Tick: %d / 24000", currentTick);

        // Day Count
        int dayCount = timeManager->GetDayCount();
        ImGui::Text("Day Count: %d", dayCount);

        // Celestial Angle (0.0-1.0)
        float celestialAngle = timeManager->GetCelestialAngle();
        ImGui::Text("Celestial Angle: %.3f", celestialAngle);

        // Compensated Celestial Angle (celestialAngle + 0.25)
        float compensatedAngle = timeManager->GetCompensatedCelestialAngle();
        ImGui::Text("Compensated Angle: %.3f", compensatedAngle);

        // Cloud Time (tick * 0.03)
        float cloudTime = timeManager->GetCloudTime();
        ImGui::Text("Cloud Time: %.2f", cloudTime);

        // ==================== Time Phase Display (Helper Info) ====================

        ImGui::Separator();
        ImGui::Text("Time Phase:");

        // Calculate time phase based on tick
        std::string phaseText;
        if (currentTick >= 0 && currentTick < 6000)
        {
            phaseText = "Sunrise -> Noon";
        }
        else if (currentTick >= 6000 && currentTick < 12000)
        {
            phaseText = "Noon -> Sunset";
        }
        else if (currentTick >= 12000 && currentTick < 18000)
        {
            phaseText = "Sunset -> Midnight";
        }
        else // 18000 - 24000
        {
            phaseText = "Midnight -> Sunrise";
        }

        ImGui::BulletText("%s", phaseText.c_str());

        ImGui::SeparatorText("Time Settings");

        static float timeScale = 1.0f;
        if (ImGui::SliderFloat("Time Speed", &timeScale, 0.0f, 100.0f, "%.1f"))
        {
            timeManager->SetTimeScale(timeScale);
        }

        if (ImGui::Button("Reset"))
        {
            timeScale = 1.0f;
            timeManager->SetTimeScale(timeScale);
        }

        ImGui::Unindent();
    }
}
