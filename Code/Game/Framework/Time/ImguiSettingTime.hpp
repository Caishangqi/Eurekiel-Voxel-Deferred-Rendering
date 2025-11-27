#pragma once

// Forward declaration
class TimeOfDayManager;

/**
 * ImguiSettingTime - Static ImGui interface for TimeOfDayManager debugging
 *
 * Purpose:
 * - Display time system parameters (tick, celestial angles, cloud time)
 * - Provide runtime adjustment controls for debugging
 * - CollapsingHeader format for clean UI organization
 *
 * Design Pattern:
 * - Static-only class (no instantiation)
 * - Stateless UI rendering (reads from TimeOfDayManager directly)
 * - Follows ImGui immediate mode paradigm
 *
 * Usage:
 * // In Game::RenderImGui():
 * if (ImGui::Begin("Game Settings"))
 * {
 *     ImguiSettingTime::Show(m_timeOfDayManager);
 * }
 * ImGui::End();
 *
 * Reference:
 * - F:/p4/Personal/SD/Engine/Code/ThirdParty/imgui/imgui_demo.cpp (Official API examples)
 * - ImGui::SliderInt, ImGui::SliderFloat, ImGui::Text, ImGui::CollapsingHeader
 */
class ImguiSettingTime
{
public:
    // [IMPORTANT] Static-only class - no instantiation allowed
    ImguiSettingTime()                                   = delete;
    ImguiSettingTime(const ImguiSettingTime&)            = delete;
    ImguiSettingTime& operator=(const ImguiSettingTime&) = delete;

    /**
     * Show - Render time system debug UI in a CollapsingHeader
     *
     * Parameters:
     * - timeManager: TimeOfDayManager instance to display/modify
     *
     * UI Layout:
     * - CollapsingHeader: "Time System"
     *   - Text: Current Tick (read-only display)
     *   - Text: Day Count (read-only display)
     *   - Text: Celestial Angle (read-only display)
     *   - Text: Compensated Celestial Angle (read-only display)
     *   - Text: Cloud Time (read-only display)
     *   - [Future] SliderInt: Tick override (for debugging)
     *   - [Future] SliderFloat: Time speed multiplier (for debugging)
     */
    static void Show(TimeOfDayManager* timeManager);
};
