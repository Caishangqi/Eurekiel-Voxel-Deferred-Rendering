/**
 * @file ImguiGameLogic.hpp
 * @brief ImGui Game Logic Tab (Time System and Gameplay Parameters)
 * @date 2025-11-26
 *
 * Features:
 * - Time system parameters (ImguiSettingTime)
 * - Future: Gameplay parameters, entity management, etc.
 * - Static-only class (no instantiation)
 *
 * Architecture:
 * ImguiGameLogic (Tab Content)
 *   └── ImguiSettingTime::Show()
 *
 * Usage:
 * @code
 * // Called by ImguiGameSettings in "Game Logic" tab
 * ImguiGameLogic::Show();
 * @endcode
 */

#pragma once

/**
 * @class ImguiGameLogic
 * @brief Static-only class for Game Logic tab content
 *
 * Responsibilities:
 * - Organize game logic UI modules
 * - Call time system UI (ImguiSettingTime)
 * - Future: Add gameplay, entity, world management UI
 */
class ImguiGameLogic
{
public:
    // [REQUIRED] Static-only class - Delete all constructors
    ImguiGameLogic()                                 = delete;
    ImguiGameLogic(const ImguiGameLogic&)            = delete;
    ImguiGameLogic& operator=(const ImguiGameLogic&) = delete;

    /**
     * @brief Show Game Logic tab content
     *
     * Current Modules:
     * - Time System (ImguiSettingTime::Show())
     *
     * Future Modules:
     * - Gameplay Parameters
     * - Entity Management
     * - World Settings
     */
    static void Show();
};
