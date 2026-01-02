/**
 * @file ImguiGameLogic.cpp
 * @brief ImGui Game Logic Tab Implementation
 * @date 2025-11-26
 */

#include "ImguiGameLogic.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Framework/Time/ImguiSettingTime.hpp"
#include "Game/Gameplay/Game.hpp"

/**
 * @brief Show Game Logic tab content
 */
void ImguiGameLogic::Show()
{
    // [MODULE 1] Time System
    ImguiSettingTime::Show(g_theGame->m_timeProvider.get());

    // [FUTURE] Add more game logic modules here
    // - Gameplay Parameters
    // - Entity Management
    // - World Settings
}
