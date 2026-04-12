/**
 * @file ImguiGameSettings.hpp
 * @brief ImGui Game Settings Main Window (Tab-based Architecture)
 * @date 2025-11-26
 *
 * Features:
 * - Main settings window with TabBar structure
 * - "Game Logic" Tab: Time system, gameplay parameters
 * - "Scene Rendering" Tab: Sky, cloud, sun/moon rendering parameters
 * - Static-only class (no instantiation)
 *
 * Architecture:
 * ImguiGameSettings (Main Window)
 *   ├── TabBar
 *   │   ├── Tab: "Game Logic"
 *   │   │   └── ImguiGameLogic::Show()
 *   │   └── Tab: "Scene Rendering"
 *   │       └── ImguiSceneRendering::Show()
 *
 * Usage:
 * @code
 * // In Game::RenderImGui()
 * ImguiGameSettings::ShowWindow(&m_showGameSettings);
 * @endcode
 */

#pragma once

/**
 * @class ImguiGameSettings
 * @brief Static-only main window for game settings with TabBar structure
 *
 * Responsibilities:
 * - Create main settings window
 * - Manage TabBar structure
 * - Coordinate Game Logic and Scene Rendering tabs
 * - Handle window visibility state
 */
class ImguiGameSettings
{
public:
    // [REQUIRED] Static-only class - Delete all constructors
    ImguiGameSettings()                                    = delete;
    ImguiGameSettings(const ImguiGameSettings&)            = delete;
    ImguiGameSettings& operator=(const ImguiGameSettings&) = delete;

    /**
     * @brief Show main game settings window with TabBar
     * @param pOpen Pointer to bool controlling window visibility (can be nullptr)
     *
     * Window Structure:
     * - Main Window: "Game Settings"
     * - TabBar with 2 tabs:
     *   1. "Game Logic" - Time system, gameplay parameters
     *   2. "Scene Rendering" - Sky, cloud, sun/moon parameters
     *
     * Tab Nesting:
     * BeginTabBar("GameSettingsTabs")
     *   ├── BeginTabItem("Game Logic")
     *   │   ├── ImguiGameLogic::Show()
     *   │   └── EndTabItem()
     *   └── BeginTabItem("Scene Rendering")
     *       ├── ImguiSceneRendering::Show()
     *       └── EndTabItem()
     * EndTabBar()
     */
    static void ShowWindow(bool* pOpen = nullptr);
};
